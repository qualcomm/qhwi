//
//
//  qhwi_android_util.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "qhwi_android_util.h"
#include <qhwi/qhwi_log_util.h>
#include <qre/text/textutil.h>

using namespace qhwi_android_util;

void GetCommandLineArgsOp::run()
{
    JNIEnv* env;
    app->activity->vm->AttachCurrentThread(&env, 0);

    // intent = this->getIntent()
    jobject thiz = app->activity->clazz;
    jclass nativeActivityClazz = env->GetObjectClass(thiz);
    jmethodID getIntentMethodID = env->GetMethodID(nativeActivityClazz, "getIntent", "()Landroid/content/Intent;");
    jobject intent = env->CallObjectMethod(thiz, getIntentMethodID);

    // cmdLineParams = intent->getStringExtra("cmdLine")
    jclass intentClazz = env->GetObjectClass(intent);
    jmethodID getStringExtraMethodId = env->GetMethodID(intentClazz, "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");
    jstring cmdLineParamsJs = (jstring)env->CallObjectMethod(intent, getStringExtraMethodId, env->NewStringUTF("cmdLine"));
    const char* cmdLineParams = cmdLineParamsJs ? env->GetStringUTFChars(cmdLineParamsJs, 0) : nullptr;

    qre::PrintfSafe(cmdLine, "%s %s", name, cmdLineParams ? cmdLineParams : "");
    QHWI_LOGI("args: %s", cmdLine);
    if (cmdLineParams)
    {
        env->ReleaseStringUTFChars(cmdLineParamsJs, cmdLineParams);
    }

    argc = split(cmdLine, &argv);
}

int GetCommandLineArgsOp::split(char* cmdLine, char*** argv)
{
    int argc = 0;
    static char* argv_[1024];
    //QRE_ASSERT_MSG(cmdLine && argv, "cmdLine != argv");

    while (*cmdLine && (*cmdLine == ' ' || *cmdLine == '\t'))
    {
        ++cmdLine;
    }

    while (*cmdLine)
    {
        //QRE_ASSERT_MSG(argc < int(std::size(argv_)), "argc less than size(argv).");

        if (*cmdLine == '\'' || *cmdLine == '"')
        {
            // quoted argument
            // TODO: allow nesting?
            char match = *cmdLine++;
            argv_[argc++] = cmdLine;
            while (*cmdLine && *cmdLine != match)
            {
                ++cmdLine;
            }
        }
        else
        {
            // non-quoted argument
            argv_[argc++] = cmdLine;
            while (*cmdLine && (*cmdLine != ' ' && *cmdLine != '\t'))
            {
                ++cmdLine;
            }
        }

        if (*cmdLine)
        {
            *cmdLine++ = '\0';
            while (*cmdLine && (*cmdLine == ' ' || *cmdLine == '\t'))
            {
                ++cmdLine;
            }
        }
    }

    argv_[argc] = nullptr;

    *argv = argv_;
    return argc;
}
