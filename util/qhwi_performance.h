//
//
//  qhwi_performance.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#pragma once

#include <qhwi/qhwi.h>
#include <qre/memory/BumpAllocator.h>
#include <qre/memory/StaticQueue.h>
#include <qre/memory/StaticVector.h>

#include <deque>
#include <queue>
#include <string>
#include <unordered_map>
#include <cstddef>
#include <stdint.h>

namespace qhwi_performance
{
class PerformanceLogManager;

class GPUTimerScope final
{
public:
    GPUTimerScope(PerformanceLogManager* pPerformanceLogManager, uint32_t metricID, qhwi::CommandList* commandList);

    ~GPUTimerScope();

private:
    PerformanceLogManager* m_pPerformanceLogManager;
    uint32_t m_metricID;
    qhwi::CommandList* m_commandList;
};

class CPUTimerScope final
{
public:
    CPUTimerScope(PerformanceLogManager* pPerformanceLogManager, uint32_t metricID);

    ~CPUTimerScope();

private:
    PerformanceLogManager* m_pPerformanceLogManager;
    uint32_t m_metricID;
};

enum class PLMCategory
{
    eApplication,
    eEnvironment,
    eCmdLineArgs,
    ePerformance,
    eCount
};

enum class PLMApplication
{
    eVersion,
    eCommitHash,
    eCount
};

enum class PLMEnvironment
{
    eHostOS,
    eCPUArch,
    eGPU,
    eNumCores,
    eTotalPhysicalMemory,
    eDateTime,
    eAPI,
    eCount
};

enum class PLMMetricAggregate
{
    eSum,
    eAverage,
    eMin,
    eMax,
    eCount
};

enum class PLMMetricUnit
{
    eCount,
    eRate,
    ePercent,
    eMillisec,
    eMicrosec,
    eBytes,
    eBytesPerSec,
    eHertz,
    eUnknown,
    eNumMetricUnits,
};

enum class PLMMetricType
{
    eShaderBased,
    eCPUTime,
    eGPUTime
};

struct PLMMetricUnitToStrMap
{
    PLMMetricUnit unit;
    const char* unitStr;
};

constexpr PLMMetricUnitToStrMap MetricUnitToStrMap[(uint32_t)PLMMetricUnit::eNumMetricUnits] = {
    { PLMMetricUnit::eCount, "" },        { PLMMetricUnit::eRate, "" },   { PLMMetricUnit::ePercent, "%" },         { PLMMetricUnit::eMillisec, "msec" },
    { PLMMetricUnit::eMicrosec, "usec" }, { PLMMetricUnit::eBytes, "B" }, { PLMMetricUnit::eBytesPerSec, "B/sec" }, { PLMMetricUnit::eHertz, "Hz" },
    { PLMMetricUnit::eUnknown, "" },
};

struct PLMMetricProperty
{
    uint32_t metricID;
    const char* clientName;
    const char* metricName;
    const char* metricDescription;
    const char* category;
    PLMMetricType metricType;
    PLMMetricUnit metricUnit;
    PLMMetricAggregate aggregateType;
};

using PLMMetricID = uint32_t;

class RealtimeMetricHistory final
{
public:
    static constexpr uint32_t s_MaxHistoryLength = 250;

    void Initialize(const char* metricName, const char* metricDescription, PLMMetricID metricID, PLMMetricUnit metricUnit);
    void Push(float value);

    void SetAutoScroll(bool autoScroll) { m_autoScroll = autoScroll; }

    void ClearHistory() { m_historyBuffer.clear(); }

    size_t GetHistorySize() const { return m_historyBuffer.size(); }

    const char* GetMetricName() const { return m_metricName; }

    const char* GetMetricDescription() const { return m_metricDescription; }

    PLMMetricUnit GetMetricUnit() const { return m_metricUnit; }

    const qre::StaticQueue<float, s_MaxHistoryLength>& GetHistory() const { return m_historyBuffer; }

private:
    const char* m_metricName = nullptr;
    const char* m_metricDescription = nullptr;
    bool m_autoScroll = false;
    PLMMetricID m_metricID = 0;
    PLMMetricUnit m_metricUnit{};
    qre::StaticQueue<float, s_MaxHistoryLength> m_historyBuffer = { 0.f };
};

class PerformanceLogManager final
{
public:
    PerformanceLogManager();
    ~PerformanceLogManager();

    void SetLogFilePath(const char* filepath);
    bool Create(const char* pVersion,
                const char* pCommitHash,
                const std::string& filename,
                const uint64_t* pFrameCount,
                uint64_t warmupFrames,
                bool showRegisteredMetrics,
                qre::BumpAllocator* pBumpAllocator);
    void Initialize(qhwi::Device* device, const char* deviceName, qhwi::CommandQueue* commandQueue);
    PLMMetricID RegisterMetric(const std::string& clientName,
                               const std::string& metricName,
                               PLMMetricType metricType,
                               PLMMetricUnit unit,
                               const std::string& metricDescription,
                               const std::string& category,
                               int frequency = 1,
                               PLMMetricAggregate aggregateType = PLMMetricAggregate::eSum,
                               uint32_t shaderBasedMetricID = 0);
    void ActivateMetrics(const std::vector<std::string>& perfClients, const std::vector<std::string>& perfCategories, const std::vector<std::string>& perfMetrics);
    bool GetPLMMetricProperyByMetricName(const char* metricName, PLMMetricProperty& metricProperty) const;
    bool GetPLMMetricProperyByMetricID(uint32_t metricID, PLMMetricProperty& metricProperty) const;
    std::span<RealtimeMetricHistory* const> GetMetricHistoryList();
    float GetAggregatedRecordedMetric(qhwi_performance::PLMMetricID metricId);
    void ShowRegisteredMetrics();

    bool AreMetricsActive(const char* clientName) const;
    bool AreShaderMetricsActive(const char* clientName = nullptr) const;

    template <typename T>
    void LogMetadata(PLMCategory category, const char* key, const T& value);

    void UpdateLastCompletedSubmissionID(qhwi::CommandList* commandList);
    void LogGPUBeginTimer(PLMMetricID metricID, qhwi::CommandList* commandList);
    void LogGPUEndTimer();
    void LogCPUBeginTimer(PLMMetricID metricID);
    void LogCPUEndTimer();

    void LogPreComputedMetric(PLMMetricID metricID, float value);

    void StartFrame();
    void ShutDown();

    qhwi::Buffer* GetShaderMetricsBuffer() { return m_shaderMetricsBuffer; }

private:
    struct MetricMetadata
    {
        std::string m_clientName;
        std::string m_metricName;
        PLMMetricType m_metricType{};
        PLMMetricUnit m_metricUnit{};
        std::string m_description;
        std::string m_metricUnitStr;
        std::string m_category;
        std::string m_header;
        std::vector<float> m_values;
        PLMMetricAggregate m_aggregateType{};
        float m_unitMultiplier = 0.f;
        unsigned int m_metricID = 0;
        int m_frequency = 0;
        uint64_t* m_metricCount = 0;
        bool m_isActive = false;
        uint32_t m_shaderBasedMetricIndex = 0;
    };

    struct MetricEntry
    {
        MetricEntry() = delete;

        MetricEntry(uint32_t metricID, uint64_t frameID, float value)
            : m_frameID(frameID)
            , m_metricID(metricID)
            , m_value(value)
        {
        }

        uint64_t m_frameID;     ///< FrameID
        PLMMetricID m_metricID; ///< Metric ID
        float m_value;          ///< Value
    };

    struct MetricTracker
    {
        enum MetricType
        {
            MetricTypeGPU,
            MetricTypeCPU,
            MetricTypeShader,
            MetricTypeInvalid
        };

        MetricTracker() = delete;

        MetricTracker(uint32_t metricID, uint64_t frameID, qhwi::CommandList* commandList, MetricType metricType, uint32_t shaderMetricReadbackBufferID)
            : m_timerQuery()
            , m_commandList(commandList)
            , m_frameID(frameID)
            , m_cpuTimerStart(0)
            , m_cpuTimerEnd(0)
            , m_metricID(metricID)
            , m_value(0)
            , m_metricType(metricType)
            , m_shaderMetricReadbackBufferID(shaderMetricReadbackBufferID)
        {
        }

        void AttachTimerQuery(qre::RefCountPtr<qhwi::TimerQuery> timerQuery)
        {
            m_timerQuery = timerQuery;
            QRE_ASSERT(m_timerQuery != nullptr);
        }

        void ResetTimerQuery(qhwi::Device* device)
        {
            // TODO: Add qhwi code
        }

        const qre::RefCountPtr<qhwi::TimerQuery> GetTimerQuery() const { return m_timerQuery; }

        void BeginTimerQuery()
        {
            QRE_ASSERT((m_metricType == MetricTypeGPU) && (m_commandList != nullptr));
            m_commandList->beginTimerQuery(m_timerQuery);
        }

        void EndTimerQuery()
        {
            QRE_ASSERT((m_metricType == MetricTypeGPU) && (m_commandList != nullptr));
            m_commandList->endTimerQuery(m_timerQuery);
        }

        void Destroy() { m_timerQuery = nullptr; }

        void SetValue(float value) { m_value = value; }

        const float& GetValue() const { return m_value; }

        MetricEntry GetMetricEntry() const { return MetricEntry(m_metricID, m_frameID, m_value); }

        void BeginCPUTimer()
        {
            QRE_ASSERT(m_metricType == MetricTypeCPU);
            m_cpuTimerStart = qre::OsTimestamp();
        }

        void EndCPUTimer()
        {
            QRE_ASSERT(m_metricType == MetricTypeCPU);
            m_cpuTimerEnd = qre::OsTimestamp();

            SetValue(static_cast<float>(qre::OsTimestampDeltaSeconds(m_cpuTimerEnd, m_cpuTimerStart)));
        }

        MetricType GetMetricType() const { return m_metricType; }

        uint32_t GetMetricID() const { return m_metricID; }

        uint32_t GetShaderMetricReadbackBufferID() const { return m_shaderMetricReadbackBufferID; }

    private:
        qre::RefCountPtr<qhwi::TimerQuery> m_timerQuery;
        qhwi::CommandList* m_commandList;
        uint64_t m_frameID;
        uint64_t m_cpuTimerStart;
        uint64_t m_cpuTimerEnd;
        uint32_t m_metricID;
        float m_value;
        MetricType m_metricType;
        uint32_t m_shaderMetricReadbackBufferID;
    };

    class RealtimeMetricsInfo final
    {
    public:
        void Initialize();
        void EnableRealtimeMetric(const char* metricName, const char* metricDescription, PLMMetricID plmMetricID, PLMMetricUnit metricUnit);
        void RecordMetricValue(PLMMetricID metricID, float value);

        std::span<RealtimeMetricHistory* const> GetMetricHistoryList() { return std::span{ m_activeMetricList, m_numActiveMetrics }; }

    private:
        static constexpr uint32_t s_MaxMetrics = 128;
        RealtimeMetricHistory m_metricHistoryList[s_MaxMetrics];
        RealtimeMetricHistory* m_activeMetricList[s_MaxMetrics];
        uint32_t m_numActiveMetrics;
    };

    template <typename T>
    struct ResourceGroup
    {
        std::vector<T> m_resource;
        uint64_t m_submissionID;
    };

    struct BufferRef
    {
        BufferRef() {}

        BufferRef(const qre::RefCountPtr<qhwi::Buffer>& v0) { v = v0; }

        inline BufferRef& operator=(const qre::RefCountPtr<qhwi::Buffer>& v0)
        {
            v = v0;
            *state = qhwi::BufferState_NoAccess;
            return *this;
        }

        inline operator qhwi::Buffer*() const { return v.Get(); }

        inline qhwi::Buffer* operator->() const { return v.Get(); }

        operator const qre::RefCountPtr<qhwi::Buffer>&() { return v; }

        std::shared_ptr<qhwi::BufferState> state = std::make_shared<qhwi::BufferState>();
        qre::RefCountPtr<qhwi::Buffer> v;
    };

    void InitializeCSV();
    void InitializeTimerQueryPool();
    void InitializeShaderMetricsBufferPool();
    void WriteToCSV();
    std::string GetFullFilename();
    float Reduce(const MetricMetadata& metric) const;
    std::string CreateStringWithValue(const char* value) const;
    template <typename T>
    std::string CreateStringWithValue(T value) const;
    void ProcessRealtimeMetrics();

    void BufferBarrier1(qhwi::CommandList* cmdList, BufferRef buffer, const qhwi::BufferState& newState);
    void BufferBarrier(qhwi::CommandList* cmdList, BufferRef buffer, const qhwi::BufferState& newState);

    static constexpr uint16_t TimerQueryPoolSize = 1024;
    static constexpr uint16_t MaxShaderBasedMetrics = 8;
    static constexpr uint16_t ShaderMetricsBufferPoolSize = MaxShaderBasedMetrics * 4;
    static constexpr float MetricUnitMultipliers[(uint32_t)PLMMetricUnit::eNumMetricUnits] = { 1.0f, 1.0f, 1.0f, 1000.0f, 1000000.0f, 1.0f, 1.0f, 1.0f, 1.0f };

    qhwi::Device* m_device;
    qhwi::CommandQueue* m_commandQueue;
    std::string m_filepath;
    std::string m_filename;
    qre::BumpAllocator* m_pBumpAllocator;
    std::array<std::unordered_map<std::string, std::string>, (int)PLMCategory::eCount> m_jsonMetadata;
    bool m_isPLMActive;
    bool m_isFileLoggingEnabled;
    std::unordered_map<uint32_t, MetricMetadata> m_metricMetadata;
    const uint64_t* m_pFrameCount;
    const std::unordered_map<PLMCategory, std::string> m_PLMCategoryMap;
    const std::unordered_map<PLMApplication, std::string> m_PLMAppMap;
    const std::unordered_map<PLMEnvironment, std::string> m_PLMEnvMap;
    const uint64_t m_bufferSizeInBytes;
    uint64_t m_lastCompletedSubmissionID;
    uint64_t m_lastSubmittedSubmissionID;
    std::deque<MetricEntry> m_recordedMetricQueue;
    uint64_t m_maxNumRecordedEntries;
    uint64_t m_warmupFrames;
    uint64_t m_nextFrameID;
    PLMMetricID m_nextMetricID;
    std::vector<MetricTracker> m_metricsCapturedThisFrame;
    std::deque<ResourceGroup<MetricTracker>> m_metricGroupTracker;
    std::deque<qre::RefCountPtr<qhwi::TimerQuery>> m_availableTimerQueryPool;
    std::vector<qre::RefCountPtr<qhwi::TimerQuery>> m_pendingResolveTimerQueryPool;
    std::vector<qre::RefCountPtr<qhwi::TimerQuery>> m_pendingResetTimerQueryPool;
    std::deque<ResourceGroup<qre::RefCountPtr<qhwi::TimerQuery>>> m_pendingResetQueue;
    qre::StaticVector<int32_t, TimerQueryPoolSize> m_activeGPUMetrics;
    qre::StaticVector<int32_t, TimerQueryPoolSize> m_activeCPUMetrics;
    RealtimeMetricsInfo m_realtimeMetricsMetadata;

    BufferRef m_shaderMetricsBuffer;
    uint32_t m_shaderMetricsBufferSize;
    std::deque<uint32_t> m_availableShaderMetricReadbackBufferPool;
    qre::StaticVector<BufferRef, ShaderMetricsBufferPoolSize> m_shaderMetricsReadbackBuffers;
};
} // namespace qhwi_performance

namespace qhwi_metrics
{
// Enums
enum class ResultCode
{
    eSuccess = 0,
    eTimeout,
    eInvalidParameter,
    eInvalidState,
    eInternalFailure
};

enum class LogLevel
{
    eInfo,
    eWarning,
    eError
};

enum class SocketType
{
    eInetTCP = 0,
    eUnixAbstract,
    eUnixNamed
};

enum class RealtimeMetricType
{
    eUnknown = 0,
    eCount,
    eRate,
    ePercent,
};

// Structs
struct SocketConfig
{
    SocketType socketType = SocketType::eInetTCP;
    int32_t userSocket = 0;
};

using LogCallbackFunc = void (*)(LogLevel level, const char* tag, const char* message);

struct Config
{
    LogCallbackFunc logCallback = nullptr;
    uint8_t disableAsyncTraceDataProcessing = 0;
    SocketConfig* traceSocketConfigGL = nullptr;
    SocketConfig* traceSocketConfigVK = nullptr;
    uint8_t enableHighFrequencyRealtime = 0;
};

using RealtimeMetricID = uint32_t;

struct RealtimeMetricProperties
{
    RealtimeMetricID metricId = 0;
    RealtimeMetricType metricType = RealtimeMetricType::eUnknown;
    const char* name = nullptr;
    const char* category = nullptr;
    const char* key = nullptr;
    const char* description = nullptr;
};

struct RealtimeData
{
    RealtimeMetricID metricId = 0;
    double value = 0.0;
};

class RealtimeMetrics
{
public:
    RealtimeMetrics();
    ~RealtimeMetrics();

    ResultCode Init();
    ResultCode Shutdown();
    ResultCode GetAvailableRealtimeMetrics(const RealtimeMetricProperties** metrics, std::size_t* count) const;
    ResultCode GetRealtimeMetricProperties(RealtimeMetricID metricId, const RealtimeMetricProperties** metric) const;
    ResultCode ValidateRealtimeMetricSelection(const RealtimeMetricID* currentMetricIds,
                                               std::size_t currentMetricCount,
                                               const RealtimeMetricID* newMetricIds,
                                               std::size_t newMetricCount,
                                               const uint8_t** metricSelectionResults) const;
    ResultCode StartRealtimeMonitor(const RealtimeMetricID* metricIds,
                                    std::size_t metricCount,
                                    const RealtimeMetricID** unavailableMetricIds,
                                    std::size_t* numUnavailableMetrics) const;
    ResultCode StopRealtimeMonitor() const;
    ResultCode GetRealtimeValues(const RealtimeData** values, std::size_t* count) const;

private:
    void* m_pil = nullptr;
};
}; // namespace qhwi_metrics
