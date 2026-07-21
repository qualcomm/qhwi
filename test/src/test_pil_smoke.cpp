//
//  test_pil_smoke.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <qhwi_performance.h>
#include <qhwi/qhwi_log_util.h>
#ifdef QHWI_PLATFORM_ANDROID
#    include <android/log.h>
#    define PIL_LOG(...) __android_log_print(ANDROID_LOG_INFO, "PIL_SMOKE", __VA_ARGS__)
#else
#    define PIL_LOG(...) QHWI_LOGI(__VA_ARGS__)
#endif

void pil_smoke_test()
{
#ifdef QHWI_ENABLE_PIL
    PIL_LOG("PIL smoke test: Init()...");
    qhwi_metrics::RealtimeMetrics pil;
    auto result = pil.Init();
    if (result != qhwi_metrics::ResultCode::eSuccess)
    {
        PIL_LOG("PIL smoke test: Init() failed — is SDP running? result=%d", (int)result);
        return;
    }
    PIL_LOG("PIL smoke test: Init() succeeded");

    const qhwi_metrics::RealtimeMetricProperties* metrics = nullptr;
    size_t count = 0;
    result = pil.GetAvailableRealtimeMetrics(&metrics, &count);
    if (result != qhwi_metrics::ResultCode::eSuccess)
    {
        PIL_LOG("PIL smoke test: GetAvailableRealtimeMetrics() failed result=%d", (int)result);
    }
    else
    {
        PIL_LOG("PIL smoke test: %zu metrics available", count);
        for (size_t i = 0; i < count && i < 5; ++i)
        {
            PIL_LOG("PIL smoke test:   [%zu] id=%u", i, metrics[i].metricId);
        }
    }

    pil.Shutdown();
    PIL_LOG("PIL smoke test: done");
#else
    PIL_LOG("PIL smoke test: skipped (QHWI_ENABLE_PIL not defined)");
#endif
}
