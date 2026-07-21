//
//
//  qhwi_log_util.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifndef __QHWI_LOG_UTIL_H__
#define __QHWI_LOG_UTIL_H__

#include <qre/os/os.h>

#define QHWI_LOGV(...) qre::OsLogMessage(qre::LogChannel_Graphics, qre::LogSeverity_Debug, __VA_ARGS__)
#define QHWI_LOGI(...) qre::OsLogMessage(qre::LogChannel_Graphics, qre::LogSeverity_Info, __VA_ARGS__)
#define QHWI_LOGW(...) qre::OsLogMessage(qre::LogChannel_Graphics, qre::LogSeverity_Warning, __VA_ARGS__)

#define QHWI_TODO_MSG(msg) QHWI_LOGI("TODO: " msg)
#define QHWI_TODO()        QHWI_LOGI("TODO")

#define QHWI_ASSERT_MSG QRE_ASSERT_MSG
#define QHWI_ASSERT     QRE_ASSERT
#define QHWI_ERR        QRE_ERROR_MSG
#define QHWI_FATAL      QRE_ERROR_MSG

#endif
