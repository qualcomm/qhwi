//
//
//  qhwi_metrics.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <qhwi_performance.h>
#include <qre/os/os.h>
#include <qre/text/textutil.h>

#ifdef QHWI_ENABLE_PIL
#    include <QProfiler/QProfilerInterface.h>
#endif

#if defined(QHWI_PLATFORM_WINDOWS)
#    define GETPROCADDR GetProcAddress
#    define PILMODULE   HMODULE
#elif defined(QHWI_PLATFORM_ANDROID)
#    define PILMODULE void*
#    include <dlfcn.h>

#    define GETPROCADDR dlsym
#endif

#define PILResultStr(x) uint8_t(ResultCode::e##x)
#define PILResult(x)    PILResultStr(x)

#define QPResultStr(x) uint8_t(qpResultCode::qpResult##x)
#define QPResult(x)    QPResultStr(x)

#define PILRealtimeMetricStr(x) uint8_t(RealtimeMetricType::e##x)
#define PILRealtimeMetric(x)    PILRealtimeMetricStr(x)

#define QPRealtimeMetricStr(x) uint8_t(qpRealtimeMetricType::qpRealtimeMetric##x)
#define QPRealtimeMetric(x)    QPRealtimeMetricStr(x)

#define PILLogLevelStr(x) uint8_t(LogLevel::e##x)
#define PILLogLevel(x)    PILLogLevelStr(x)

#define QPLogLevelStr(x) uint8_t(qpLogLevel::qpLog##x)
#define QPLogLevel(x)    QPLogLevelStr(x)

#define PILSocketTypeStr(x) uint8_t(SocketType::e##x)
#define PILSocketType(x)    PILSocketTypeStr(x)

#define QPSocketTypeStr(x) uint8_t(qpSocketType::qpSocket##x)
#define QPSocketType(x)    QPSocketTypeStr(x)

using namespace qhwi_metrics;
#ifdef QHWI_ENABLE_PIL

static_assert(PILResult(Success) == QPResult(Success));
static_assert(PILResult(Timeout) == QPResult(Timeout));
static_assert(PILResult(InvalidParameter) == QPResult(InvalidParameter));
static_assert(PILResult(InvalidState) == QPResult(InvalidState));
static_assert(PILResult(InternalFailure) == QPResult(InternalFailure));
static_assert(sizeof(ResultCode) == sizeof(qpResultCode));

static_assert(PILRealtimeMetric(Unknown) == QPRealtimeMetric(Unknown));
static_assert(PILRealtimeMetric(Count) == QPRealtimeMetric(Count));
static_assert(PILRealtimeMetric(Rate) == QPRealtimeMetric(Rate));
static_assert(PILRealtimeMetric(Percent) == QPRealtimeMetric(Percent));
static_assert(sizeof(RealtimeMetricType) == sizeof(qpRealtimeMetricType));

static_assert(PILLogLevel(Info) == QPLogLevel(Info));
static_assert(PILLogLevel(Warning) == QPLogLevel(Warning));
static_assert(PILLogLevel(Error) == QPLogLevel(Error));
static_assert(sizeof(LogLevel) == sizeof(qpLogLevel));

static_assert(PILSocketType(InetTCP) == QPSocketType(InetTCP));
static_assert(PILSocketType(UnixAbstract) == QPSocketType(UnixAbstract));
static_assert(PILSocketType(UnixNamed) == QPSocketType(UnixNamed));
static_assert(sizeof(SocketType) == sizeof(qpSocketType));

// Init/Shutdown
using PFN_qprofiler_Init = qpResultCode (*)(const qpConfig* cfg, int32_t* version);
using PFN_qprofiler_Shutdown = qpResultCode (*)();

// Realtime metrics
using PFN_qprofiler_GetAvailableRealtimeMetrics = qpResultCode (*)(const qpRealtimeMetricProperties** metrics, size_t* count);
using PFN_qprofiler_GetRealtimeMetricProperties = qpResultCode (*)(qpRealtimeMetricID metricId, const qpRealtimeMetricProperties** metric);
using PFN_qprofiler_ValidateRealtimeMetricSelection = qpResultCode (*)(const qpRealtimeMetricID* currentMetricIds,
                                                                       size_t currentMetricCount,
                                                                       const qpRealtimeMetricID* newMetricIds,
                                                                       size_t newMetricCount,
                                                                       const uint8_t** metricSelectionResults);
using PFN_qprofiler_StartRealtimeMonitor = qpResultCode (*)(const qpRealtimeMetricID* metricIds,
                                                            size_t metricCount,
                                                            const qpRealtimeMetricID** unavailableMetricIds,
                                                            size_t* numUnavailableMetrics);
using PFN_qprofiler_StopRealtimeMonitor = qpResultCode (*)();
using PFN_qprofiler_GetRealtimeValues = qpResultCode (*)(const qpRealtimeData** values, size_t* count);

// Proc addresses
static PFN_qprofiler_Init s_qprofilerInit;
static PFN_qprofiler_Shutdown s_qprofilerShutdown;

static PFN_qprofiler_GetAvailableRealtimeMetrics s_qprofilerGetAvailableRealtimeMetrics;
static PFN_qprofiler_GetRealtimeMetricProperties s_qprofilerGetRealtimeMetricProperties;
static PFN_qprofiler_ValidateRealtimeMetricSelection s_qprofilerValidateRealtimeMetricSelection;
static PFN_qprofiler_StartRealtimeMonitor s_qprofilerStartRealtimeMonitor;
static PFN_qprofiler_StopRealtimeMonitor s_qprofilerStopRealtimeMonitor;
static PFN_qprofiler_GetRealtimeValues s_qprofilerGetRealtimeValues;

#endif // QHWI_ENABLE_PIL

RealtimeMetrics::RealtimeMetrics() {}

RealtimeMetrics::~RealtimeMetrics() {}

ResultCode RealtimeMetrics::Init()
{
    ResultCode result = ResultCode::eSuccess;
    m_pil = nullptr;

#ifdef QHWI_ENABLE_PIL
#    if defined(QHWI_PLATFORM_WINDOWS)
    // Static anchor variable to get module handle
    static int g_ModuleAnchor = 0;

    HMODULE thisModule = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)(reinterpret_cast<LPCVOID>(&g_ModuleAnchor)), &thisModule);

    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(thisModule, modulePath, MAX_PATH);

    if (wchar_t* lastSlash = wcsrchr(modulePath, L'\\'))
    {
        *lastSlash = L'\0'; // strip filename
    }

    // Resolve symlink/junction to real path
    wchar_t realPath[MAX_PATH];
    HANDLE directoryHandle = CreateFileW(modulePath,
                                         GENERIC_READ,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                         nullptr,
                                         OPEN_EXISTING,
                                         FILE_FLAG_BACKUP_SEMANTICS, // Required for directories
                                         nullptr);

    std::wstring resolvedDir(modulePath); // fallback if resolution fails
    if (directoryHandle != INVALID_HANDLE_VALUE)
    {
        DWORD len = GetFinalPathNameByHandleW(directoryHandle, realPath, MAX_PATH, FILE_NAME_NORMALIZED);
        CloseHandle(directoryHandle);
        directoryHandle = nullptr;

        if (len > 0 && len < MAX_PATH)
        {
            resolvedDir.assign(realPath);
            // Strip \\?\ prefix if present
            if (resolvedDir.rfind(L"\\\\?\\", 0) == 0)
            {
                resolvedDir = resolvedDir.substr(4);
            }
        }
    }

    std::wstring dllPath = resolvedDir + L"\\PIL\\QProfilerInterface.dll";

    m_pil = LoadLibraryW(dllPath.c_str());
    QRE_ASSERT_MSG(m_pil != NULL, "Unable to load QProfilerInterface.dll");
#    elif defined(QHWI_PLATFORM_ANDROID)
    m_pil = dlopen("libQProfilerInterface.so", RTLD_LAZY);
    if (m_pil == nullptr)
    {
        const char* err = dlerror();
        QRE_ASSERT_MSG(false, err ? err : "dlopen(libQProfilerInterface.so) failed");
        return ResultCode::eInternalFailure;
    }
#    endif

    s_qprofilerInit = (PFN_qprofiler_Init)GETPROCADDR(static_cast<PILMODULE>(m_pil), "qprofiler_Init");
    QRE_ASSERT(s_qprofilerInit != nullptr);

    s_qprofilerShutdown = (PFN_qprofiler_Shutdown)GETPROCADDR(static_cast<PILMODULE>(m_pil), "qprofiler_Shutdown");
    QRE_ASSERT(s_qprofilerShutdown != nullptr);

    s_qprofilerGetAvailableRealtimeMetrics = (PFN_qprofiler_GetAvailableRealtimeMetrics)GETPROCADDR(static_cast<PILMODULE>(m_pil), "qprofiler_GetAvailableRealtimeMetrics");
    QRE_ASSERT(s_qprofilerGetAvailableRealtimeMetrics != nullptr);

    s_qprofilerGetRealtimeMetricProperties = (PFN_qprofiler_GetRealtimeMetricProperties)GETPROCADDR(static_cast<PILMODULE>(m_pil), "qprofiler_GetRealtimeMetricProperties");
    QRE_ASSERT(s_qprofilerGetRealtimeMetricProperties != nullptr);

    s_qprofilerValidateRealtimeMetricSelection =
        (PFN_qprofiler_ValidateRealtimeMetricSelection)GETPROCADDR(static_cast<PILMODULE>(m_pil), "qprofiler_ValidateRealtimeMetricSelection");
    QRE_ASSERT(s_qprofilerValidateRealtimeMetricSelection != nullptr);

    s_qprofilerStartRealtimeMonitor = (PFN_qprofiler_StartRealtimeMonitor)GETPROCADDR(static_cast<PILMODULE>(m_pil), "qprofiler_StartRealtimeMonitor");
    QRE_ASSERT(s_qprofilerStartRealtimeMonitor != nullptr);

    s_qprofilerStopRealtimeMonitor = (PFN_qprofiler_StopRealtimeMonitor)GETPROCADDR(static_cast<PILMODULE>(m_pil), "qprofiler_StopRealtimeMonitor");
    QRE_ASSERT(s_qprofilerStopRealtimeMonitor != nullptr);

    s_qprofilerGetRealtimeValues = (PFN_qprofiler_GetRealtimeValues)GETPROCADDR(static_cast<PILMODULE>(m_pil), "qprofiler_GetRealtimeValues");
    QRE_ASSERT(s_qprofilerGetRealtimeValues != nullptr);

    if (s_qprofilerInit)
    {
        qpConfig config = {};

        // TODO: Enable high frequency realtime. High frequency polling is returning zeroes for all metrics on Android
        config.enableHighFrequencyRealtime = false;
        result = static_cast<ResultCode>(s_qprofilerInit(&config, nullptr));
    }
#endif // QHWI_ENABLE_PIL

    return result;
}

ResultCode RealtimeMetrics::Shutdown()
{
    ResultCode result = ResultCode::eSuccess;

#ifdef QHWI_ENABLE_PIL
    if (s_qprofilerShutdown)
    {
        result = static_cast<ResultCode>(s_qprofilerShutdown());
    }

    if (m_pil)
    {
#    if defined(QHWI_PLATFORM_WINDOWS)
        FreeLibrary(static_cast<HMODULE>(m_pil));
#    elif defined(QHWI_PLATFORM_ANDROID)
        dlclose(m_pil);
#    endif

        s_qprofilerInit = nullptr;
        s_qprofilerShutdown = nullptr;
        s_qprofilerGetAvailableRealtimeMetrics = nullptr;
        s_qprofilerGetRealtimeMetricProperties = nullptr;
        s_qprofilerValidateRealtimeMetricSelection = nullptr;
        s_qprofilerStartRealtimeMonitor = nullptr;
        s_qprofilerStopRealtimeMonitor = nullptr;
        s_qprofilerGetRealtimeValues = nullptr;

        m_pil = nullptr;
    }
#endif // QHWI_ENABLE_PIL

    return result;
}

ResultCode RealtimeMetrics::GetAvailableRealtimeMetrics(const RealtimeMetricProperties** metrics, size_t* count) const
{
    ResultCode result = ResultCode::eSuccess;

#ifdef QHWI_ENABLE_PIL
    if (s_qprofilerGetAvailableRealtimeMetrics)
    {
        result = static_cast<ResultCode>(s_qprofilerGetAvailableRealtimeMetrics(reinterpret_cast<const qpRealtimeMetricProperties**>(metrics), count));
    }
    else
#endif // QHWI_ENABLE_PIL
    {
        *count = 0;
    }

    return result;
}

ResultCode RealtimeMetrics::GetRealtimeMetricProperties(RealtimeMetricID metricId, const RealtimeMetricProperties** metric) const
{
    ResultCode result = ResultCode::eSuccess;

#ifdef QHWI_ENABLE_PIL
    if (s_qprofilerGetRealtimeMetricProperties)
    {
        result = static_cast<ResultCode>(s_qprofilerGetRealtimeMetricProperties(metricId, reinterpret_cast<const qpRealtimeMetricProperties**>(metric)));
    }
#endif // QHWI_ENABLE_PIL

    return result;
}

ResultCode RealtimeMetrics::ValidateRealtimeMetricSelection(const RealtimeMetricID* currentMetricIds,
                                                            size_t currentMetricCount,
                                                            const RealtimeMetricID* newMetricIds,
                                                            size_t newMetricCount,
                                                            const uint8_t** metricSelectionResults) const
{
    ResultCode result = ResultCode::eSuccess;

#ifdef QHWI_ENABLE_PIL
    if (s_qprofilerValidateRealtimeMetricSelection)
    {
        result = static_cast<ResultCode>(s_qprofilerValidateRealtimeMetricSelection(currentMetricIds, currentMetricCount, newMetricIds, newMetricCount, metricSelectionResults));
    }
#endif // QHWI_ENABLE_PIL

    return result;
}

ResultCode RealtimeMetrics::StartRealtimeMonitor(const RealtimeMetricID* metricIds,
                                                 size_t metricCount,
                                                 const RealtimeMetricID** unavailableMetricIds,
                                                 size_t* numUnavailableMetrics) const
{
    ResultCode result = ResultCode::eSuccess;

#ifdef QHWI_ENABLE_PIL
    if (s_qprofilerStartRealtimeMonitor)
    {
        result = static_cast<ResultCode>(s_qprofilerStartRealtimeMonitor(metricIds, metricCount, unavailableMetricIds, numUnavailableMetrics));
    }
#endif // QHWI_ENABLE_PIL

    return result;
}

ResultCode RealtimeMetrics::StopRealtimeMonitor() const
{
    ResultCode result = ResultCode::eSuccess;

#ifdef QHWI_ENABLE_PIL
    if (s_qprofilerStopRealtimeMonitor)
    {
        result = static_cast<ResultCode>(s_qprofilerStopRealtimeMonitor());
    }
#endif // QHWI_ENABLE_PIL

    return result;
}

ResultCode RealtimeMetrics::GetRealtimeValues(const RealtimeData** values, size_t* count) const
{
    ResultCode result = ResultCode::eSuccess;

#ifdef QHWI_ENABLE_PIL
    if (s_qprofilerGetRealtimeValues)
    {
        result = static_cast<ResultCode>(s_qprofilerGetRealtimeValues(reinterpret_cast<const qpRealtimeData**>(values), count));
    }
    else
#endif // QHWI_ENABLE_PIL
    {
        *count = 0;
    }

    return result;
}
