//
//
//  test_app.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear

#include "test_app.h"
#include <sstream>

#define ADD_QHWI_TEST(s)                                                                                                                                                           \
    if ((testsParam.empty() || contains(testsParam, std::string(#s))) && !contains(excludeTestsParam, std::string(#s)))                                                            \
    {                                                                                                                                                                              \
        for (const auto& graphicsBackendStr : backends)                                                                                                                            \
        {                                                                                                                                                                          \
            GraphicsBackend graphicsBackend;                                                                                                                                       \
            fromString(graphicsBackendStr.c_str(), graphicsBackend);                                                                                                               \
            extern UnitTest* create_test_##s(GraphicsBackend backend, UnitTest::Config config);                                                                                    \
            std::unique_ptr<UnitTest> test(create_test_##s(graphicsBackend, config));                                                                                              \
            tests.emplace_back(std::move(test));                                                                                                                                   \
        }                                                                                                                                                                          \
    }

#ifdef QHWI_PLATFORM_ANDROID
#    include <qhwi_android_util.h>
using namespace qhwi_android_util;
#    include <fstream>
#    include <qhwi/qhwi_android_window.h>
#endif
using namespace qhwi;
using namespace test_util;

TestApp::TestApp(int argc, const char* const* argv)
{
    QHWI_LOGI("argc: %d", argc);
    for (int j = 0; j < argc; j++)
    {
        QHWI_LOGI("argv[%d]: %s", j, argv[j]);
    }
    cxxopts::Options opts("qhwi_test", "Run qhwi unit tests");
    std::string defaultGraphicsBackends =
#ifdef QHWI_BACKEND_D3D12
        "d3d12,"
#endif
#ifdef QHWI_BACKEND_VULKAN
        "vk,"
#endif
        ;
    defaultGraphicsBackends.pop_back();

    // clang-format off
    opts.add_options()
        ("tests", "Tests", cxxopts::value<std::vector<std::string>>()->default_value(""))
        ("exclude-tests", "Exclude tests", cxxopts::value<std::vector<std::string>>()->default_value(""))
        ("onscreen", "Render onscreen", cxxopts::value<bool>()->default_value("true"))
        ("capture", "Enable GPU capture", cxxopts::value<bool>()->default_value("false"))
#ifndef NDEBUG
        ("debug", "Enable debug layer", cxxopts::value<bool>()->default_value("true"))
#else
        ("debug", "Enable debug layer", cxxopts::value<bool>()->default_value("false"))
#endif
        ("width", "output surface width", cxxopts::value<uint32_t>()->default_value("720"))
        ("height", "output surface height", cxxopts::value<uint32_t>()->default_value("480"))
        ("gen-refs", "generate reference images / data", cxxopts::value<bool>()->default_value("false"))
        ("backends", "Graphics backends", cxxopts::value<std::vector<std::string>>()->default_value(defaultGraphicsBackends))
        ("max-frames", "Maximum frames.  0: unlimited", cxxopts::value<uint32_t>()->default_value("0"))
        ("emulate-uma", "Emulate UMA (Unified Memory Architecture)", cxxopts::value<bool>()->default_value("false"))
        ("disable-d3d-enhanced-barriers", "Disable d3d12 enhanced barriers", cxxopts::value<bool>()->default_value("false"));
    // clang-format on

    params = opts.parse(argc, argv);

    testsParam = params["tests"].as<std::vector<std::string>>();
    if (testsParam.size() == 1 && testsParam[0] == "")
    {
        testsParam.clear();
    }
    excludeTestsParam = params["exclude-tests"].as<std::vector<std::string>>();
    backends = params["backends"].as<std::vector<std::string>>();

    config = { .clearColor = { 0.0f, 0.0f, 0.0f, 1.0f },
               .onscreen = params["onscreen"].as<bool>(),
               .enableGpuCapture = params["capture"].as<bool>(),
               .debug = params["debug"].as<bool>(),
               .w = params["width"].as<uint32_t>(),
               .h = params["height"].as<uint32_t>(),
               .maxFrames = params["max-frames"].as<uint32_t>(),
               .genRefs = params["gen-refs"].as<bool>(),
               .emulateUma = params["emulate-uma"].as<bool>(),
               .disableD3dEnhancedBarriers = params["disable-d3d-enhanced-barriers"].as<bool>() };
#include "qhwi_test_list.inc"
    if (tests.empty())
    {
        QHWI_FATAL("no tests found");
    }
}

#ifdef QHWI_PLATFORM_ANDROID

struct Config
{
    void initDefault()
    {
        numParams = 1;
        strcpy(paramsData[0], "qhwi_test");
        params[0] = paramsData[0];
    }

    static const int MaxParams = 16;
    static const int MaxParamSize = 128;
    int numParams = 0;
    char paramsData[MaxParams][MaxParamSize];
    char* params[MaxParams];
};

struct TestAppState
{
    TestApp* app = nullptr;
    int currentTest = 0;
    bool windowInitialized = false;
    int nextTestRequested = 0;
};

static FileUtil::ResultCode parseConfig(const char* str, Config& config)
{
    std::stringstream in(str);
    int& i = config.numParams;
    while (in >> config.paramsData[i])
    {
        config.params[i] = config.paramsData[i];
        i++;
    }
    return FileUtil::ResultCode::eSuccess;
}

static void readConfigFile(const char* configFilePath, Config& config)
{
    FileData configFile;
    auto configFileFound = FileUtil::readFile(configFilePath, configFile, FileUtil::Flags(FileUtil::Flag::eQuiet));
    if (configFileFound == qhwi::FileUtil::ResultCode::eSuccess)
    {
        parseConfig((const char*)configFile.data, config);
    }
    else
    {
        config.initDefault();
    }
}

static void onAppCmd(android_app* app, int32_t cmd)
{
    TestAppState* testAppState = (TestAppState*)app->userData;
    switch (cmd)
    {
    case APP_CMD_INIT_WINDOW:
    {
        ext::SetANativeWindow(app->window);
        testAppState->windowInitialized = true;
        break;
    }
    }
}

static int32_t onInputEvent(android_app* app, AInputEvent* event)
{
    int32_t type = AInputEvent_getType(event);
    TestAppState* testAppState = (TestAppState*)app->userData;
    if (type == AINPUT_EVENT_TYPE_MOTION)
    {
        int32_t action = AMotionEvent_getAction(event);
        uint32_t actionFlags = action & AMOTION_EVENT_ACTION_MASK;
        switch (actionFlags)
        {
        case AMOTION_EVENT_ACTION_UP:
            testAppState->nextTestRequested = 1;
            break;
        };
    };
    return 0;
}

void createTestApp(android_app* state, std::unique_ptr<TestApp>& app, Config& config)
{
    int argc;
    char** argv;
    GetCommandLineArgsOp op(state, "qhwi_test", argc, argv);
    op.run();

    app.reset(new TestApp(argc, argv));
}

void createTestApp(std::unique_ptr<TestApp>& app, Config& config)
{
    app.reset(new TestApp(config.numParams, config.params));
}

static void nextTest(std::unique_ptr<TestApp>& app, int& currentTestIndex, UnitTest*& currentTest, Config& config)
{
    currentTest->waitUntilCompleted();
    app->tests[currentTestIndex].reset();
    currentTestIndex++;
    if (currentTestIndex == int(app->tests.size()))
    {
        currentTestIndex = 0;
        createTestApp(app, config);
    }
    currentTest = app->tests[currentTestIndex].get();
}

static void processEvents(android_app* state)
{
    int ident = 0;
    int events = 0;
    android_poll_source* source = nullptr;
    while ((ident = ALooper_pollAll(0, nullptr, &events, (void**)&source)) >= 0)
    {
        if (source != nullptr)
        {
            source->process(state, source);
        }
        if (state->destroyRequested != 0)
        {
            return;
        }
    }
}

extern void pil_smoke_test();

void android_main(android_app* state)
{
    FileUtil::setAndroidAssetManager(state->activity->assetManager);
    Config config{};
    readConfigFile(QHWI_DATA_DIR "/../config.txt", config);

    std::unique_ptr<TestApp> app;
    TestAppState testAppState;
    auto& currentTestIndex = testAppState.currentTest;
    state->userData = &testAppState;
    state->onAppCmd = onAppCmd;
    state->onInputEvent = onInputEvent;

    pil_smoke_test();
    ResultCode resultCode = ResultCode::eErrorOther;
    while (true)
    {
        if (!app)
        {
            createTestApp(state, app, config);
        }
        ResultCode testResultCode;
        UnitTest* currentTest = app->tests[currentTestIndex].get();

        processEvents(state);

        if (testAppState.nextTestRequested)
        {
            nextTest(app, currentTestIndex, currentTest, config);
            testAppState.nextTestRequested = false;
        }
        if (!currentTest->isInitialized() && testAppState.windowInitialized)
        {
            testResultCode = currentTest->init();
            QHWI_LOGI("test: %s %s", currentTest->getName(), testResultCode == ResultCode::eSuccess ? "initialized" : "init failed");
            if (testResultCode != ResultCode::eSuccess)
            {
                resultCode = testResultCode;
                nextTest(app, currentTestIndex, currentTest, config);
                continue;
            }
        }
        if (!currentTest->isInitialized())
        {
            continue;
        }
        if (currentTest->isFinished())
        {
            testResultCode = currentTest->compareOutput();
            QHWI_LOGI("test %s %s", currentTest->getName(), testResultCode == ResultCode::eSuccess ? "passed" : "failed");
            if (testResultCode != ResultCode::eSuccess)
            {
                resultCode = testResultCode;
            }
            nextTest(app, currentTestIndex, currentTest, config);
            continue;
        }
        currentTest->runIteration();
    }
}
#else
int main(int argc, char** argv)
{
    qre::OsSetLogConsole(true);
    TestApp app(argc, argv);
    ResultCode resultCode = ResultCode::eSuccess;
    for (auto& test : app.tests)
    {
        QHWI_LOGI("test: %s", test->getName());
        ResultCode testResultCode = test->init();
        if (testResultCode != ResultCode::eSuccess)
        {
            resultCode = testResultCode;
            continue;
        }
        while (!test->isFinished())
        {
            test->runIteration();
        }
        testResultCode = test->compareOutput();
        QHWI_LOGI("test %s %s", test->getName(), testResultCode == ResultCode::eSuccess ? "passed" : "failed");
        if (testResultCode != ResultCode::eSuccess)
        {
            resultCode = testResultCode;
        }
        test.reset();
    }
    return int(resultCode);
}
#endif
