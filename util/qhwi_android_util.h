//
//
//  qhwi_android_util.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#pragma once

#include <android_native_app_glue.h>

namespace qhwi_android_util
{
class GetCommandLineArgsOp
{
public:
    GetCommandLineArgsOp(android_app* app, const char* name, int& argc, char**& argv)
        : app(app)
        , name(name)
        , argc(argc)
        , argv(argv)
    {
    }

    void run();

private:
    int split(char* cmdLine, char*** argv);
    android_app* app;
    const char* name;
    int& argc;
    char**& argv;
    char cmdLine[4096];
};
} // namespace qhwi_android_util