//
//
//  qhwi_performance.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "qhwi_performance.h"

#include <qre/os/os.h>
#if defined(QHWI_PLATFORM_ANDROID) || defined(QHWI_PLATFORM_LINUX)
#    include <sys/sysinfo.h>
#    include <unistd.h>
#elif defined(QHWI_PLATFORM_DARWIN) || defined(QHWI_PLATFORM_IOS)
#    include <sys/sysctl.h>
#    include <sys/types.h>
#    include <unistd.h>
#endif

#include <fstream>
#include <inttypes.h>
#include <locale>
#include <numeric>
#include <algorithm>
#include <time.h>
#include <unordered_set>
#include <qre/thirdparty/nlohmann/json.hpp>

#define QHWI_SCOPE_MARKER(commandList, marker) qhwi::ScopedMarker QRE_CONCAT(scopeMarker, __COUNTER__)(commandList, marker)

using CommandListRef = qre::RefCountPtr<qhwi::CommandList>;
using CommandListPtr = qhwi::CommandList*;
using TimerQueryRef = qre::RefCountPtr<qhwi::TimerQuery>;
using TimerQueryPtr = qhwi::TimerQuery*;

qhwi_performance::GPUTimerScope::GPUTimerScope(qhwi_performance::PerformanceLogManager* pPerformanceLogManager, uint32_t metricID, qhwi::CommandList* commandList)
    : m_pPerformanceLogManager(pPerformanceLogManager)
    , m_metricID(metricID)
    , m_commandList(commandList)
{
    m_pPerformanceLogManager->LogGPUBeginTimer(m_metricID, m_commandList);
}

qhwi_performance::GPUTimerScope::~GPUTimerScope()
{
    m_pPerformanceLogManager->LogGPUEndTimer();
}

qhwi_performance::CPUTimerScope::CPUTimerScope(qhwi_performance::PerformanceLogManager* pPerformanceLogManager, uint32_t metricID)
    : m_pPerformanceLogManager(pPerformanceLogManager)
    , m_metricID(metricID)
{
    m_pPerformanceLogManager->LogCPUBeginTimer(m_metricID);
}

qhwi_performance::CPUTimerScope::~CPUTimerScope()
{
    m_pPerformanceLogManager->LogCPUEndTimer();
}

qhwi_performance::PerformanceLogManager::PerformanceLogManager()
    : m_device(nullptr)
    , m_commandQueue(nullptr)
    , m_filepath("")
    , m_filename("")
    , m_pBumpAllocator()
    , m_jsonMetadata()
    , m_isPLMActive(false)
    , m_isFileLoggingEnabled(false)
    , m_metricMetadata()
    , m_pFrameCount(nullptr)
    , m_PLMCategoryMap{ { qhwi_performance::PLMCategory::eApplication, "application" },
                        { qhwi_performance::PLMCategory::eEnvironment, "environment" },
                        { qhwi_performance::PLMCategory::eCmdLineArgs, "args" } }
    , m_PLMAppMap{ { qhwi_performance::PLMApplication::eVersion, "version" }, { qhwi_performance::PLMApplication::eCommitHash, "commit_hash" } }
    , m_PLMEnvMap{ { qhwi_performance::PLMEnvironment::eAPI, "api" },
                   { qhwi_performance::PLMEnvironment::eCPUArch, "arch" },
                   { qhwi_performance::PLMEnvironment::eDateTime, "date" },
                   { qhwi_performance::PLMEnvironment::eGPU, "gpu" },
                   { qhwi_performance::PLMEnvironment::eHostOS, "host_os" },
                   { qhwi_performance::PLMEnvironment::eNumCores, "num_logical_processors" },
                   { qhwi_performance::PLMEnvironment::eTotalPhysicalMemory, "memory_in_kilobyte" } }
    , m_bufferSizeInBytes(2560000)
    , m_lastCompletedSubmissionID(0)
    , m_lastSubmittedSubmissionID(0)
    , m_recordedMetricQueue()
    , m_maxNumRecordedEntries(0)
    , m_warmupFrames(0)
    , m_nextFrameID(0)
    , m_nextMetricID(0)
    , m_metricsCapturedThisFrame()
    , m_metricGroupTracker()
    , m_availableTimerQueryPool()
    , m_pendingResolveTimerQueryPool()
    , m_pendingResetTimerQueryPool()
    , m_pendingResetQueue()
    , m_activeGPUMetrics()
    , m_activeCPUMetrics()
    , m_shaderMetricsBufferSize(0)
{
}

qhwi_performance::PerformanceLogManager::~PerformanceLogManager() {}

void qhwi_performance::PerformanceLogManager::SetLogFilePath(const char* filepath)
{
    // Required while running on android to specify activity->internalDataPath
    m_filepath = filepath;
}

bool qhwi_performance::PerformanceLogManager::Create(const char* pVersion,
                                                     const char* pCommitHash,
                                                     const std::string& filename,
                                                     const uint64_t* pFrameCount,
                                                     uint64_t warmUpFrames,
                                                     bool showRegisteredMetrics,
                                                     qre::BumpAllocator* pBumpAllocator)
{
    m_isPLMActive = (showRegisteredMetrics == false);
    m_isFileLoggingEnabled = (filename.length() > 0);
    m_filename = filename;
    m_pFrameCount = pFrameCount;
    m_warmupFrames = warmUpFrames;
    m_nextFrameID = warmUpFrames;
    m_pBumpAllocator = pBumpAllocator;

    if (m_isPLMActive)
    {
        m_realtimeMetricsMetadata.Initialize();

        if (m_isFileLoggingEnabled)
        {
            auto versionIt = m_PLMAppMap.find(PLMApplication::eVersion);
            QHWI_ASSERT(versionIt != m_PLMAppMap.end());
            LogMetadata(PLMCategory::eApplication, versionIt->second.c_str(), pVersion);

            auto commitHashIt = m_PLMAppMap.find(PLMApplication::eCommitHash);
            QHWI_ASSERT(commitHashIt != m_PLMAppMap.end());
            LogMetadata(PLMCategory::eApplication, commitHashIt->second.c_str(), pCommitHash);
        }
    }

    return m_isPLMActive;
}

std::string qhwi_performance::PerformanceLogManager::CreateStringWithValue(const char* value) const
{
    return std::string(value);
}

template <typename T>
std::string qhwi_performance::PerformanceLogManager::CreateStringWithValue(T value) const
{
    return std::to_string(value);
}

template <typename T>
void qhwi_performance::PerformanceLogManager::LogMetadata(qhwi_performance::PLMCategory category, const char* key, const T& value)
{
    switch (category)
    {
    case PLMCategory::eApplication:
        m_jsonMetadata[(int)PLMCategory::eApplication][key] = CreateStringWithValue(value);
        break;

    case PLMCategory::eCmdLineArgs:
        m_jsonMetadata[(int)PLMCategory::eCmdLineArgs][key] = CreateStringWithValue(value);
        break;

    case PLMCategory::eEnvironment:
        m_jsonMetadata[(int)PLMCategory::eEnvironment][key] = CreateStringWithValue(value);
        break;

    default:
        break;
    }
}

void qhwi_performance::PerformanceLogManager::ActivateMetrics(const std::vector<std::string>& perfClients,
                                                              const std::vector<std::string>& perfCategories,
                                                              const std::vector<std::string>& perfMetrics)
{
    // Clients, categories and metrics received from the command line
    // are stored as strings in the JSON object (comma separated when more than one metric is provided)
    // Extracting individual elements from comma separated string for activation
    for (auto& metricMetadata : m_metricMetadata)
    {
        for (auto& client : perfClients)
        {
            if (metricMetadata.second.m_clientName.compare(client) == 0)
            {
                metricMetadata.second.m_isActive = true;
                m_realtimeMetricsMetadata.EnableRealtimeMetric(metricMetadata.second.m_metricName.c_str(),
                                                               metricMetadata.second.m_description.c_str(),
                                                               metricMetadata.second.m_metricID,
                                                               metricMetadata.second.m_metricUnit);
            }
        }

        for (auto& category : perfCategories)
        {
            if (metricMetadata.second.m_category.compare(category) == 0)
            {
                metricMetadata.second.m_isActive = true;
                m_realtimeMetricsMetadata.EnableRealtimeMetric(metricMetadata.second.m_metricName.c_str(),
                                                               metricMetadata.second.m_description.c_str(),
                                                               metricMetadata.second.m_metricID,
                                                               metricMetadata.second.m_metricUnit);
            }
        }

        for (auto& metric : perfMetrics)
        {
            size_t pos = metric.find("::");
            if (pos == std::string::npos)
            {
                // No delimiter in the performance metric.  Just compare the metric names.
                if (metricMetadata.second.m_metricName.compare(metric) == 0)
                {
                    metricMetadata.second.m_isActive = true;
                    m_realtimeMetricsMetadata.EnableRealtimeMetric(metricMetadata.second.m_metricName.c_str(),
                                                                   metricMetadata.second.m_description.c_str(),
                                                                   metricMetadata.second.m_metricID,
                                                                   metricMetadata.second.m_metricUnit);
                }
            }
            else
            {
                // Compare both the client name and the metric name with the performance metric
                if (metric.compare(0, pos, metricMetadata.second.m_clientName) == 0 && metric.compare(pos + 2, metric.length(), metricMetadata.second.m_metricName) == 0)
                {
                    metricMetadata.second.m_isActive = true;
                    m_realtimeMetricsMetadata.EnableRealtimeMetric(metricMetadata.second.m_metricName.c_str(),
                                                                   metricMetadata.second.m_description.c_str(),
                                                                   metricMetadata.second.m_metricID,
                                                                   metricMetadata.second.m_metricUnit);
                }
            }
        }
    }
}

// Are any metrics active for this client
bool qhwi_performance::PerformanceLogManager::AreMetricsActive(const char* clientName) const
{
    for (auto& metric : m_metricMetadata)
    {
        if (metric.second.m_clientName == clientName && metric.second.m_isActive)
        {
            return true;
        }
    }
    return false;
}

bool qhwi_performance::PerformanceLogManager::AreShaderMetricsActive(const char* clientName) const
{
    if (m_isPLMActive)
    {
        for (auto& metric : m_metricMetadata)
        {
            if (clientName == nullptr || metric.second.m_clientName == clientName)
            {
                if (metric.second.m_isActive && metric.second.m_metricType == PLMMetricType::eShaderBased)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

bool qhwi_performance::PerformanceLogManager::GetPLMMetricProperyByMetricName(const char* metricName, PLMMetricProperty& metricProperty) const
{
    for (const std::pair<const uint32_t, qhwi_performance::PerformanceLogManager::MetricMetadata>& it : m_metricMetadata)
    {
        const MetricMetadata& metric = it.second;

        if (strcmp(metric.m_metricName.c_str(), metricName) == 0)
        {
            metricProperty.aggregateType = metric.m_aggregateType;
            metricProperty.category = metric.m_category.c_str();
            metricProperty.clientName = metric.m_clientName.c_str();
            metricProperty.metricDescription = metric.m_description.c_str();
            metricProperty.metricID = metric.m_metricID;
            metricProperty.metricName = metric.m_metricName.c_str();
            metricProperty.metricType = metric.m_metricType;
            metricProperty.metricUnit = metric.m_metricUnit;

            return true;
        }
    }

    return false;
}

bool qhwi_performance::PerformanceLogManager::GetPLMMetricProperyByMetricID(uint32_t metricID, qhwi_performance::PLMMetricProperty& metricProperty) const
{
    auto it = m_metricMetadata.find(metricID);

    if (it == m_metricMetadata.end())
    {
        return false;
    }
    else
    {
        const MetricMetadata& metric = it->second;

        metricProperty.aggregateType = metric.m_aggregateType;
        metricProperty.category = metric.m_category.c_str();
        metricProperty.clientName = metric.m_clientName.c_str();
        metricProperty.metricDescription = metric.m_description.c_str();
        metricProperty.metricID = metric.m_metricID;
        metricProperty.metricName = metric.m_metricName.c_str();
        metricProperty.metricType = metric.m_metricType;
        metricProperty.metricUnit = metric.m_metricUnit;

        return true;
    }
}

static constexpr const char* MetricUnitString(qhwi_performance::PLMMetricUnit unit)
{
    switch (unit)
    {
    case qhwi_performance::PLMMetricUnit::eCount:
        return "count";
    case qhwi_performance::PLMMetricUnit::eMicrosec:
        return "usec";
    case qhwi_performance::PLMMetricUnit::eMillisec:
        return "msec";
    case qhwi_performance::PLMMetricUnit::ePercent:
        return "percent";
    case qhwi_performance::PLMMetricUnit::eRate:
        return "per sec";
    case qhwi_performance::PLMMetricUnit::eBytes:
        return "bytes";
    case qhwi_performance::PLMMetricUnit::eBytesPerSec:
        return "bytes per sec";
    case qhwi_performance::PLMMetricUnit::eHertz:
        return "hertz";
    case qhwi_performance::PLMMetricUnit::eUnknown:
    default:
        QHWI_FATAL("Unknown metric unit");
    }
}

qhwi_performance::PLMMetricID qhwi_performance::PerformanceLogManager::RegisterMetric(const std::string& clientName,
                                                                                      const std::string& metricName,
                                                                                      qhwi_performance::PLMMetricType metricType,
                                                                                      qhwi_performance::PLMMetricUnit unit,
                                                                                      const std::string& metricDescription,
                                                                                      const std::string& category,
                                                                                      int frequency,
                                                                                      qhwi_performance::PLMMetricAggregate aggregateType,
                                                                                      uint32_t shaderBasedMetricID)
{
    QHWI_ASSERT((metricType == PLMMetricType::eShaderBased && shaderBasedMetricID < MaxShaderBasedMetrics) || (metricType != PLMMetricType::eShaderBased));

    MetricMetadata metricMetadata;
    PLMMetricID metricID = m_nextMetricID;

    metricMetadata.m_clientName = clientName;
    metricMetadata.m_metricName = metricName;
    metricMetadata.m_metricType = metricType;
    metricMetadata.m_metricUnit = unit;
    metricMetadata.m_metricUnitStr = MetricUnitString(unit);
    metricMetadata.m_category = category;
    metricMetadata.m_description = metricDescription;
    metricMetadata.m_frequency = frequency;
    metricMetadata.m_aggregateType = aggregateType;
    metricMetadata.m_unitMultiplier = MetricUnitMultipliers[(uint32_t)unit];
    metricMetadata.m_metricID = metricID;
    metricMetadata.m_header = clientName + "::" + metricName + "::(" + MetricUnitString(unit) + ")";
    metricMetadata.m_isActive = false;
    metricMetadata.m_metricCount = nullptr;
    metricMetadata.m_shaderBasedMetricIndex = shaderBasedMetricID;

    auto [it, inserted] = m_metricMetadata.insert({ metricID, std::move(metricMetadata) });
    if (inserted)
    {
        it->second.m_values.reserve(TimerQueryPoolSize);
    }

    ++m_nextMetricID;
    return metricID;
}

void qhwi_performance::PerformanceLogManager::LogGPUBeginTimer(qhwi_performance::PLMMetricID metricID, qhwi::CommandList* commandList)
{
    if (m_isPLMActive) [[unlikely]]
    {
        auto metric = m_metricMetadata.find(metricID);
        if (metric != m_metricMetadata.end())
        {
            if (metric->second.m_isActive)
            {
                auto timerQuery = m_availableTimerQueryPool.front();
                m_pendingResolveTimerQueryPool.push_back(timerQuery);
                m_availableTimerQueryPool.pop_front();

                auto index = static_cast<int32_t>(m_metricsCapturedThisFrame.size());
                auto& iterator = m_metricsCapturedThisFrame.emplace_back(metricID, *m_pFrameCount, commandList, MetricTracker::MetricType::MetricTypeGPU, 0);
                iterator.AttachTimerQuery(timerQuery);
                iterator.BeginTimerQuery();

                m_activeGPUMetrics.push_back(index);
            }
            else
            {
                m_activeGPUMetrics.push_back(-1);
            }
        }
    }
}

void qhwi_performance::PerformanceLogManager::LogGPUEndTimer()
{
    if (m_isPLMActive) [[unlikely]]
    {
        if (!m_activeGPUMetrics.empty())
        {
            auto index = m_activeGPUMetrics.back();

            if (index != -1)
            {
                m_metricsCapturedThisFrame[index].EndTimerQuery();
            }

            m_activeGPUMetrics.pop_back();
        }
        else
        {
            QHWI_FATAL("No active GPU timer to end.");
        }
    }
}

void qhwi_performance::PerformanceLogManager::LogPreComputedMetric(qhwi_performance::PLMMetricID metricID, float value)
{
    if (m_isPLMActive) [[unlikely]]
    {
        auto it = m_metricMetadata.find(metricID);

        if (it != m_metricMetadata.end() && it->second.m_isActive)
        {
            auto& iterator = m_metricsCapturedThisFrame.emplace_back(metricID, *m_pFrameCount, nullptr, MetricTracker::MetricType::MetricTypeCPU, 0);
            iterator.SetValue(value);
        }
    }
}

void qhwi_performance::PerformanceLogManager::LogCPUBeginTimer(qhwi_performance::PLMMetricID metricID)
{
    if (m_isPLMActive) [[unlikely]]
    {
        auto it = m_metricMetadata.find(metricID);

        if (it != m_metricMetadata.end())
        {
            if (it->second.m_isActive)
            {
                auto index = static_cast<int32_t>(m_metricsCapturedThisFrame.size());
                auto& iterator = m_metricsCapturedThisFrame.emplace_back(metricID, *m_pFrameCount, nullptr, MetricTracker::MetricType::MetricTypeCPU, 0);
                iterator.BeginCPUTimer();
                m_activeCPUMetrics.push_back(index);
            }
            else
            {
                m_activeCPUMetrics.push_back(-1);
            }
        }
        else
        {
            QHWI_FATAL("No active CPU timer to end.");
        }
    }
}

void qhwi_performance::PerformanceLogManager::LogCPUEndTimer()
{
    if (m_isPLMActive) [[unlikely]]
    {
        if (!m_activeCPUMetrics.empty())
        {
            auto index = m_activeCPUMetrics.back();

            if (index != -1)
            {
                m_metricsCapturedThisFrame[index].EndCPUTimer();
            }

            m_activeCPUMetrics.pop_back();
        }
        else
        {
            QHWI_FATAL("No CPU timer to end %s %s", __FILE__, __LINE__);
        }
    }
}

std::string qhwi_performance::PerformanceLogManager::GetFullFilename()
{
    if (!m_filepath.empty())
    {
        m_filepath += "/";
    }

    return m_filepath + m_filename;
}

void qhwi_performance::PerformanceLogManager::ShowRegisteredMetrics()
{
    auto AggregateTypeToString = [](PLMMetricAggregate aggregateType)
    {
        switch (aggregateType)
        {
        case PLMMetricAggregate::eSum:
            return "Sum";

        case PLMMetricAggregate::eAverage:
            return "Average";

        case PLMMetricAggregate::eMin:
            return "Min";

        case PLMMetricAggregate::eMax:
            return "Max";

        case PLMMetricAggregate::eCount:
            return "Count";

        default:
            QHWI_FATAL("Unknown aggregate function");
        }
    };

    for (auto metricIt = m_metricMetadata.begin(); metricIt != m_metricMetadata.end(); ++metricIt)
    {
        qre::OsLogMessage(qre::LogChannel_System,
                          qre::LogSeverity_Info,
                          "\n\
            Client Name : %s\n\
            Metric Name : %s\n\
            Unit        : %s\n\
            Category    : %s\n\
            Description : %s\n\
            Frequency   : %d\n\
            Aggregate Fn: %s\n",
                          metricIt->second.m_clientName.c_str(),
                          metricIt->second.m_metricName.c_str(),
                          metricIt->second.m_metricUnitStr.c_str(),
                          metricIt->second.m_category.c_str(),
                          metricIt->second.m_description.c_str(),
                          metricIt->second.m_frequency,
                          AggregateTypeToString(metricIt->second.m_aggregateType));
    }
}

void qhwi_performance::PerformanceLogManager::Initialize(qhwi::Device* device, const char* deviceName, qhwi::CommandQueue* commandQueue)
{
    if (m_isPLMActive && (m_PLMCategoryMap.find(PLMCategory::eEnvironment) != m_PLMCategoryMap.end()))
    {
        m_device = device;
        m_commandQueue = commandQueue;
        m_maxNumRecordedEntries = m_bufferSizeInBytes / sizeof(MetricEntry);

        // Initialize timer pool
        InitializeTimerQueryPool();

        // Initialize shader metrics buffer pool
        InitializeShaderMetricsBufferPool();

        // Graphics API
        if (m_PLMEnvMap.find(PLMEnvironment::eAPI) != m_PLMEnvMap.end())
        {
            auto envIt = m_PLMEnvMap.find(PLMEnvironment::eAPI);
            const char* apiName;
            switch (device->getGraphicsBackend())
            {
            case qhwi::GraphicsBackend::eDirectX12:
                apiName = "DX12";
                break;

            case qhwi::GraphicsBackend::eVulkan:
                apiName = "VK";
                break;

            default:
                apiName = "UNKNOWN";
                QHWI_FATAL("Unknown backend");
                break;
            }

            LogMetadata(PLMCategory::eEnvironment, envIt->second.c_str(), apiName);
        }
        else
        {
            QHWI_FATAL(0);
        }

        // Date time
        if (m_PLMEnvMap.find(PLMEnvironment::eDateTime) != m_PLMEnvMap.end())
        {
            auto dateTimeIt = m_PLMEnvMap.find(PLMEnvironment::eDateTime);

            char buf[64];
            std::time_t now = std::time(nullptr);
#if defined(QHWI_PLATFORM_WINDOWS)
            std::locale::global(std::locale("en_US.utf8"));
            tm time;
            localtime_s(&time, &now);
            std::strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Z", &time);
#else
            tm* pTime;
            pTime = std::localtime(&now);
            std::strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Z", pTime);
#endif
            LogMetadata(PLMCategory::eEnvironment, dateTimeIt->second.c_str(), buf);
        }
        else
        {
            QHWI_FATAL(0);
        }

        // Platform information
        if (m_PLMEnvMap.find(PLMEnvironment::eHostOS) != m_PLMEnvMap.end() && m_PLMEnvMap.find(PLMEnvironment::eCPUArch) != m_PLMEnvMap.end() &&
            m_PLMEnvMap.find(PLMEnvironment::eGPU) != m_PLMEnvMap.end() && m_PLMEnvMap.find(PLMEnvironment::eNumCores) != m_PLMEnvMap.end() &&
            m_PLMEnvMap.find(PLMEnvironment::eTotalPhysicalMemory) != m_PLMEnvMap.end())
        {
            auto hostOsIt = m_PLMEnvMap.find(PLMEnvironment::eHostOS);
            auto cpuIt = m_PLMEnvMap.find(PLMEnvironment::eCPUArch);
            auto gpuIt = m_PLMEnvMap.find(PLMEnvironment::eGPU);
            auto numCoresIt = m_PLMEnvMap.find(PLMEnvironment::eNumCores);
            auto phyMemIt = m_PLMEnvMap.find(PLMEnvironment::eTotalPhysicalMemory);

            uint64_t totalMemoryInKilobytes;
            uint64_t numberOfProcessors;

            const char* cpuArchName;
            const char* hostOsName;

#if defined(QHWI_PLATFORM_WINDOWS)
            SYSTEM_INFO systemInfo{};

            GetSystemInfo(&systemInfo);
            GetPhysicallyInstalledSystemMemory(&totalMemoryInKilobytes);

            hostOsName = "WINDOWS";
            numberOfProcessors = systemInfo.dwNumberOfProcessors;

            switch (systemInfo.wProcessorArchitecture)
            {
            case PROCESSOR_ARCHITECTURE_AMD64:
                cpuArchName = "AMD64";
                break;

            case PROCESSOR_ARCHITECTURE_ARM:
                cpuArchName = "ARM";
                break;

            case PROCESSOR_ARCHITECTURE_ARM64:
                cpuArchName = "ARM64";
                break;

            case PROCESSOR_ARCHITECTURE_IA64:
                cpuArchName = "IA64";
                break;

            case PROCESSOR_ARCHITECTURE_INTEL:
                cpuArchName = "INTEL";
                break;

            case PROCESSOR_ARCHITECTURE_UNKNOWN:
            default:
                cpuArchName = "UNKNOWN";
                break;
            }

#else
#    if defined(QHWI_PLATFORM_LINUX)
            hostOsName = "LINUX";

            struct sysinfo sysInfo{};

            sysinfo(&sysInfo);
            totalMemoryInKilobytes = sysInfo.totalram >> 10u;
            numberOfProcessors = sysconf(_SC_NPROCESSORS_CONF);
#    elif defined(QHWI_PLATFORM_ANDROID)
            hostOsName = "ANDROID";

            struct sysinfo sysInfo{};

            sysinfo(&sysInfo);
            totalMemoryInKilobytes = sysInfo.totalram >> 10u;
            numberOfProcessors = sysconf(_SC_NPROCESSORS_CONF);
#    elif defined(QHWI_PLATFORM_DARWIN) || defined(QHWI_PLATFORM_IOS)
            hostOsName = "MACOS/IOS";
            size_t len = sizeof(totalMemoryInKilobytes);
            sysctlbyname("hw.memsize", &totalMemoryInKilobytes, &len, nullptr, 0);
            sysctlbyname("hw.ncpu", &numberOfProcessors, &len, nullptr, 0);

            totalMemoryInKilobytes >>= 10u;
#    else
#        error "Unknown platform"
#    endif

#    ifdef __arm__
            cpuArchName = "ARM";
#    endif

#    ifdef __aarch64__
            cpuArchName = "ARM64";
#    endif

#    ifdef __i386__
            cpuArchName = "INTEL";
#    endif

#    ifdef __x86_64__
            cpuArchName = "AMD64";
#    endif
#endif

            LogMetadata(PLMCategory::eEnvironment, cpuIt->second.c_str(), cpuArchName);
            LogMetadata(PLMCategory::eEnvironment, numCoresIt->second.c_str(), numberOfProcessors);
            LogMetadata(PLMCategory::eEnvironment, hostOsIt->second.c_str(), hostOsName);
            LogMetadata(PLMCategory::eEnvironment, phyMemIt->second.c_str(), totalMemoryInKilobytes);
            LogMetadata(PLMCategory::eEnvironment, gpuIt->second.c_str(), deviceName);
        }
        else
        {
            QHWI_FATAL(0);
        }

        // Write metadata to JSON
        if (m_isFileLoggingEnabled)
        {
            nlohmann::ordered_json rootJson;
            nlohmann::ordered_json appJson(m_jsonMetadata[(int)PLMCategory::eApplication]);
            nlohmann::ordered_json cmdLineArgsJson(m_jsonMetadata[(int)PLMCategory::eCmdLineArgs]);
            nlohmann::ordered_json envInfoJson(m_jsonMetadata[(int)PLMCategory::eEnvironment]);

            rootJson[m_PLMCategoryMap.find(PLMCategory::eApplication)->second.c_str()] = appJson;
            rootJson[m_PLMCategoryMap.find(PLMCategory::eCmdLineArgs)->second.c_str()] = cmdLineArgsJson;
            rootJson[m_PLMCategoryMap.find(PLMCategory::eEnvironment)->second.c_str()] = envInfoJson;

            std::ofstream outFile(GetFullFilename() + ".json");
            if (outFile.is_open())
            {
                outFile << rootJson.dump(4);
                outFile.close();
            }
            else
            {
                qre::OsLogMessage(qre::LogChannel_System, qre::LogSeverity_Error, "Cannot write Performance logs");
            }
        }
    }
}

void qhwi_performance::PerformanceLogManager::InitializeTimerQueryPool()
{
    if (m_isPLMActive)
    {
        CommandListRef commandList = m_device->createCommandList(m_commandQueue);

        m_availableTimerQueryPool.resize(TimerQueryPoolSize);
        m_metricsCapturedThisFrame.reserve(TimerQueryPoolSize);
        m_pendingResolveTimerQueryPool.reserve(TimerQueryPoolSize);

        commandList->open();
        {
            QHWI_SCOPE_MARKER(commandList, "Initialize Timer Query Pool");

            for (auto& query : m_availableTimerQueryPool)
            {
                query = m_device->createTimerQuery();
                commandList->resetTimerQueryPool(query);
            }
        }

        commandList->close();

        m_commandQueue->executeCommandList(commandList);
        m_commandQueue->waitForIdle();
    }
}

void qhwi_performance::PerformanceLogManager::InitializeShaderMetricsBufferPool()
{
    if (m_isPLMActive)
    {
        m_shaderMetricsBufferSize = sizeof(uint32_t) * MaxShaderBasedMetrics;

        m_shaderMetricsBuffer =
            m_device->createBuffer({ .usage = qhwi::BufferUsage::BufferUsage_RWByteAddressBuffer, .size = m_shaderMetricsBufferSize, .name = "ShaderMetricsBuffer" });

        m_shaderMetricsReadbackBuffers.resize(ShaderMetricsBufferPoolSize);
        for (uint32_t s = 0; s < ShaderMetricsBufferPoolSize; s++)
        {
            // Create metrics readback buffer
            m_shaderMetricsReadbackBuffers[s] =
                m_device->createBuffer({ .heapType = qhwi::HeapType::eDownload, .usage = 0, .size = m_shaderMetricsBufferSize, .name = "MetricsReadbackBuffer" });

            m_availableShaderMetricReadbackBufferPool.push_back(s);
        }
    }
}

void qhwi_performance::PerformanceLogManager::UpdateLastCompletedSubmissionID(qhwi::CommandList* commandList)
{
    if (m_isPLMActive) [[unlikely]]
    {
        // Get the last submitted command list submissionID on the graphics queue
        // Note: this may be overwritten on DX if timer queries are used
        m_lastSubmittedSubmissionID = m_commandQueue->getLastSubmittedID();

        // Get the last completed command list submissionID on the graphics queue
        // Note: this may be overwritten on DX if timer queries are used
        m_lastCompletedSubmissionID = m_commandQueue->getLastCompletedID();
        // Drop frames if current frame count < warmUpFrames
        if (m_isFileLoggingEnabled && (*m_pFrameCount == m_warmupFrames))
        {
            InitializeCSV();
        }

        // Clear vector before recording realtime values
        for (auto& it : m_metricMetadata)
        {
            it.second.m_values.clear();
        }

        // ResolveQueryData is a commandList operation, required by DX12 only.
        // Has no effect in VK
        if (!m_pendingResolveTimerQueryPool.empty())
        {
            for (auto it = m_pendingResolveTimerQueryPool.begin(); it != m_pendingResolveTimerQueryPool.end(); ++it)
            {
                commandList->resolveTimerQueryData(*it);
            }

            m_pendingResolveTimerQueryPool.clear();
        }

        if (AreShaderMetricsActive())
        {
            // Create a command list and copy the shader metrics buffer to a readback buffer
            uint32_t shaderMetricsBufferID = m_availableShaderMetricReadbackBufferPool.front();

            BufferBarrier(commandList, m_shaderMetricsBuffer, qhwi::BufferState_Transfer_Src);
            BufferBarrier(commandList, m_shaderMetricsReadbackBuffers[shaderMetricsBufferID], qhwi::BufferState_Transfer_Dst);
            commandList->beginTransferPass();
            commandList->copyBuffer(m_shaderMetricsReadbackBuffers[shaderMetricsBufferID], m_shaderMetricsBuffer);

            // Also clear the shader metric buffer for the next time it's used
            BufferBarrier(commandList, m_shaderMetricsBuffer, qhwi::BufferState_Transfer_Dst);
            commandList->clear(m_shaderMetricsBuffer, 0);
            commandList->endTransferPass();
        }

        // Process completed metrics for this frame
        while (!m_metricGroupTracker.empty())
        {
            auto& front = m_metricGroupTracker.front();

            // Check to make sure that this group's command list was submitted before the last completed command list submission for the graphics queue.
            if (front.m_submissionID <= m_lastCompletedSubmissionID)
            {
                // For shader metrics, just copy the RW buffer once
                bool shaderMetricsbufferCopied = false;
                uint32_t shaderMetricsReadbackBufferID = UINT32_MAX;
                uint32_t* shaderMetricsDataRaw = nullptr;

                // Go through each of the metrics in the group
                for (auto it = front.m_resource.begin(); it != front.m_resource.end(); ++it)
                {
                    if (it->GetMetricType() == MetricTracker::MetricTypeGPU)
                    {
                        TimerQueryRef query = it->GetTimerQuery();
                        m_pendingResetTimerQueryPool.push_back(query);
                        float timerSeconds = 0.0f;
                        QHWI_ASSERT(m_device->getTimerQueryTimeInSec(query, m_commandQueue, true, timerSeconds));
                        it->SetValue(timerSeconds);
                    }
                    else if (it->GetMetricType() == MetricTracker::MetricTypeShader)
                    {
                        // At least one shader based metric is active..
                        if (!shaderMetricsbufferCopied)
                        {
                            // We need to copy the shader metrics buffer.
                            // First get the buffer ID
                            shaderMetricsReadbackBufferID = it->GetShaderMetricReadbackBufferID();

                            // map the buffer so we can copy the shader metric values from it.
                            shaderMetricsDataRaw = (uint32_t*)m_shaderMetricsReadbackBuffers[shaderMetricsReadbackBufferID]->map();
                            // Put the shader metrics buffer back on the dequeue so it can be reused.
                            m_availableShaderMetricReadbackBufferPool.push_back(shaderMetricsReadbackBufferID);

                            // Set the flag so we don't wind up copying the buffer a second time for this frame
                            shaderMetricsbufferCopied = true;
                        }
                        // Set the shader metric value from the mapping
                        uint32_t metricIndex = m_metricMetadata[it->GetMetricID()].m_shaderBasedMetricIndex;
                        it->SetValue((float)shaderMetricsDataRaw[metricIndex]);
                    }

                    MetricEntry entry = it->GetMetricEntry();
                    // Add the metric to the recorded metric queue so it gets written to the csv
                    if (it->GetMetricEntry().m_frameID >= m_warmupFrames)
                    {
                        m_recordedMetricQueue.emplace_back(entry);
                    }

                    m_metricMetadata[entry.m_metricID].m_values.push_back(entry.m_value);
                    QHWI_ASSERT(m_metricMetadata[entry.m_metricID].m_metricID == entry.m_metricID);
                } // group metrics

                // Record values for plotting metrics in UI
                ProcessRealtimeMetrics();

                // If we  mapped the readback buffer, unmap it
                if (shaderMetricsbufferCopied)
                {
                    QHWI_ASSERT(shaderMetricsReadbackBufferID != UINT32_MAX);
                    m_shaderMetricsReadbackBuffers[shaderMetricsReadbackBufferID]->unmap();
                }
                m_metricGroupTracker.pop_front();
            }
            else
            {
                break;
            }
        }

        // Add metrics captured in current frame to the queue
        ResourceGroup<MetricTracker> metricGroup;
        metricGroup.m_submissionID = m_lastSubmittedSubmissionID;
        metricGroup.m_resource = std::move(m_metricsCapturedThisFrame);

        if (!metricGroup.m_resource.empty())
        {
            m_metricGroupTracker.push_back(std::move(metricGroup));
        }

        // If shader metrics are active, then make sure the next frame uses a different shader metric buffer
        if (AreShaderMetricsActive())
        {
            m_availableShaderMetricReadbackBufferPool.pop_front();
        }

        // Reset timer query pool. Required by Vulkan
        // Has no effect in DX12
        if (!m_pendingResolveTimerQueryPool.empty())
        {
            for (auto it = m_pendingResetTimerQueryPool.begin(); it != m_pendingResetTimerQueryPool.end(); ++it)
            {
                commandList->resetTimerQueryPool(*it);
            }
        }

        // Process timers that have been reset and ready to be reused
        while (!m_pendingResetQueue.empty())
        {
            auto& front = m_pendingResetQueue.front();
            if (front.m_submissionID <= m_lastCompletedSubmissionID)
            {
                QHWI_SCOPE_MARKER(commandList, "Reset Timer Query Pool");
                for (auto it = front.m_resource.begin(); it != front.m_resource.end(); ++it)
                {
                    commandList->resetTimerQueryPool(*it);
                    m_availableTimerQueryPool.push_back(*it);
                }

                m_pendingResetQueue.pop_front();
            }
            else
            {
                break;
            }
        }

        // Add timers waiting to be reset to the queue
        ResourceGroup<TimerQueryRef> timerGroup;
        timerGroup.m_submissionID = m_lastSubmittedSubmissionID;
        timerGroup.m_resource = std::move(m_pendingResetTimerQueryPool);

        if (!timerGroup.m_resource.empty())
        {
            m_pendingResetQueue.push_back(std::move(timerGroup));
        }

        if (m_isFileLoggingEnabled && (m_recordedMetricQueue.size() > m_maxNumRecordedEntries))
        {
            WriteToCSV();
        }
    }
}

void qhwi_performance::PerformanceLogManager::InitializeCSV()
{
    // Write to CSV file
    FILE* fp;
    fopen_s(&fp, (GetFullFilename() + ".csv").c_str(), "w");
    QHWI_ASSERT(fp != nullptr);

    fprintf(fp, "FrameID");

    for (auto& metric : m_metricMetadata)
    {
        if (metric.second.m_isActive)
        {
            fprintf(fp, ",%s::%s(%s)", metric.second.m_clientName.c_str(), metric.second.m_metricName.c_str(), metric.second.m_metricUnitStr.c_str());
        }
    }

    fprintf(fp, "\n");
    fclose(fp);
}

void qhwi_performance::PerformanceLogManager::ProcessRealtimeMetrics()
{
    for (auto& metric : m_metricMetadata)
    {
        if (metric.second.m_isActive == true)
        {
            if (metric.second.m_values.size() > 0)
            {
                m_realtimeMetricsMetadata.RecordMetricValue(metric.second.m_metricID, Reduce(metric.second));
            }
            else
            {
                m_realtimeMetricsMetadata.RecordMetricValue(metric.second.m_metricID, 0.0);
            }
        }
    }
}

float qhwi_performance::PerformanceLogManager::Reduce(const MetricMetadata& metric) const
{
    float aggregateValue = 0.0;

    switch (metric.m_aggregateType)
    {
    case PLMMetricAggregate::eSum:
    case PLMMetricAggregate::eAverage:
        aggregateValue = std::reduce(metric.m_values.begin(), metric.m_values.end(), 0.0f, std::plus<float>());

        if ((metric.m_aggregateType == PLMMetricAggregate::eAverage) && (metric.m_values.size() > 0))
        {
            aggregateValue /= metric.m_values.size();
        }
        break;

    case PLMMetricAggregate::eCount:
        aggregateValue = static_cast<float>(metric.m_values.size());
        break;

    case PLMMetricAggregate::eMin:
        if (!metric.m_values.empty())
        {
            aggregateValue = *std::min_element(metric.m_values.begin(), metric.m_values.end());
        }
        break;

    case PLMMetricAggregate::eMax:
        if (!metric.m_values.empty())
        {
            aggregateValue = *std::max_element(metric.m_values.begin(), metric.m_values.end());
        }
        break;

    default:
        QHWI_FATAL("Invalid aggregate type");
        break;
    }

    aggregateValue *= metric.m_unitMultiplier;

    return aggregateValue;
}

void qhwi_performance::PerformanceLogManager::WriteToCSV()
{
    // Write to CSV file, one line at a time
    FILE* fp;
    std::string csvFilename = GetFullFilename() + ".csv";
    fopen_s(&fp, csvFilename.c_str(), "a");
    QHWI_ASSERT_MSG(fp != nullptr, "Unable to open file %s", csvFilename.c_str());

    while (!m_recordedMetricQueue.empty())
    {
        // Clear existing values in actively tracked metrics
        for (auto& metric : m_metricMetadata)
        {
            metric.second.m_values.clear();
        }

        fprintf(fp, "%" PRIu64, m_nextFrameID);
        uint32_t metricsProcessed = 0;

        // Copy the metrics corresponding to m_nextFrameID
        for (auto it = m_recordedMetricQueue.begin(); it != m_recordedMetricQueue.end(); ++it)
        {
            if (it->m_frameID != m_nextFrameID)
            {
                break;
            }
            else
            {
                m_metricMetadata[it->m_metricID].m_values.push_back(it->m_value);
                ++metricsProcessed;
            }
        }

        ++m_nextFrameID;

        for (uint32_t i = 0; i < metricsProcessed; i++)
        {
            m_recordedMetricQueue.pop_front();
        }

        // Write to file
        for (auto& metric : m_metricMetadata)
        {
            if (metric.second.m_isActive == true)
            {
                if (metric.second.m_values.size() > 0)
                {
                    fprintf(fp, ",%.4f", Reduce(metric.second));
                }
                else
                {
                    fprintf(fp, ",NA");
                }
            }
        }

        fprintf(fp, "\n");
    }

    fclose(fp);
}

std::span<qhwi_performance::RealtimeMetricHistory* const> qhwi_performance::PerformanceLogManager::GetMetricHistoryList()
{
    return m_realtimeMetricsMetadata.GetMetricHistoryList();
}

void qhwi_performance::PerformanceLogManager::StartFrame()
{
    if (m_isPLMActive) [[unlikely]]
    {
        // Add Shader based metrics to metrics capture list
        for (auto& metric : m_metricMetadata)
        {
            if (metric.second.m_isActive && metric.second.m_metricType == PLMMetricType::eShaderBased)
            {
                m_metricsCapturedThisFrame.emplace_back(metric.second.m_metricID,
                                                        *m_pFrameCount,
                                                        nullptr,
                                                        MetricTracker::MetricType::MetricTypeShader,
                                                        m_availableShaderMetricReadbackBufferPool.front());
            }
        }
    }
}

void qhwi_performance::PerformanceLogManager::ShutDown()
{
    if (m_isPLMActive)
    {
        CommandListRef commandList = m_device->createCommandList(m_commandQueue);
        commandList->open();
        while (!m_metricGroupTracker.empty() || !m_pendingResetQueue.empty())
        {
            UpdateLastCompletedSubmissionID(commandList);
        }
        commandList->close();
        m_commandQueue->executeCommandList(commandList);

        QHWI_ASSERT(m_availableTimerQueryPool.size() == TimerQueryPoolSize);

        if (m_isFileLoggingEnabled)
        {
            WriteToCSV();
        }

        m_availableTimerQueryPool.clear();

        m_shaderMetricsBuffer = nullptr;
        m_shaderMetricsReadbackBuffers.clear();

        m_isPLMActive = false;
    }
}

void qhwi_performance::PerformanceLogManager::RealtimeMetricsInfo::Initialize()
{
    m_numActiveMetrics = 0;

    for (uint32_t i = 0; i < s_MaxMetrics; ++i)
    {
        m_activeMetricList[i] = nullptr;
    }
}

void qhwi_performance::PerformanceLogManager::RealtimeMetricsInfo::EnableRealtimeMetric(const char* metricName,
                                                                                        const char* metricDescription,
                                                                                        PLMMetricID metricID,
                                                                                        PLMMetricUnit metricUnit)
{
    RealtimeMetricHistory* metricHistory = &m_metricHistoryList[metricID];
    metricHistory->Initialize(metricName, metricDescription, metricID, metricUnit);

    m_activeMetricList[m_numActiveMetrics++] = metricHistory;
}

void qhwi_performance::PerformanceLogManager::RealtimeMetricsInfo::RecordMetricValue(PLMMetricID metricID, float value)
{
    m_metricHistoryList[metricID].Push(value);
}

void qhwi_performance::RealtimeMetricHistory::Initialize(const char* metricName, const char* metricDescription, PLMMetricID metricID, PLMMetricUnit metricUnit)
{
    m_metricName = metricName;
    m_metricDescription = metricDescription;
    m_metricID = metricID;
    m_metricUnit = metricUnit;
    m_autoScroll = true;
}

void qhwi_performance::RealtimeMetricHistory::Push(float value)
{
    if (m_autoScroll)
    {
        m_historyBuffer.force_push(value);
    }
}

void qhwi_performance::PerformanceLogManager::BufferBarrier1(qhwi::CommandList* cmdList,
                                                             qhwi_performance::PerformanceLogManager::BufferRef buffer,
                                                             const qhwi::BufferState& newState)
{
    // If old state and new state is unordered access, we still need a barrier to handle WAW, RAW, WAR hazards
    // (write after write, read after write, write after read)
    if ((newState.access != qhwi::AccessMode_UnorderedAccess) && (*buffer.state == newState))
    {
        return;
    }
    auto& s0 = buffer.state;
    const auto& s1 = newState;
    cmdList->addBufferBarrier({ buffer, *s0, s1 });
    *s0 = s1;
}

void qhwi_performance::PerformanceLogManager::BufferBarrier(qhwi::CommandList* cmdList,
                                                            qhwi_performance::PerformanceLogManager::BufferRef buffer,
                                                            const qhwi::BufferState& newState)
{
    if (buffer->automaticBarriersEnabled())
    {
        return;
    }
    BufferBarrier1(cmdList, buffer, newState);
}

float qhwi_performance::PerformanceLogManager::GetAggregatedRecordedMetric(qhwi_performance::PLMMetricID metricId)
{
    if (m_filename.size())
    {
        return 0.0;
    }

    // Clear existing values in actively tracked metrics
    bool found = false;
    for (auto& metric : m_metricMetadata)
    {
        if (metric.second.m_metricID == metricId)
        {
            metric.second.m_values.clear();
            found = true;
        }
    }
    if (!found)
    {
        return 0.0;
    }

    for (auto it = m_recordedMetricQueue.begin(); it != m_recordedMetricQueue.end(); ++it)
    {
        if (it->m_metricID == metricId)
        {
            m_metricMetadata[it->m_metricID].m_values.push_back(it->m_value);
        }
    }

    for (auto& metric : m_metricMetadata)
    {
        if (metric.second.m_metricID == metricId)
        {
            if (metric.second.m_values.size() > 0)
            {
                return Reduce(metric.second);
            }
        }
    }
    return 0.0;
}
