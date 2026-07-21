//
//
//  qhwi_android_window.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <qhwi/qhwi_android_window.h>

static ANativeWindow* s_ANativeWindow = nullptr;

qhwi::ext::AndroidWindow::AndroidWindow(const Description& desc)
    : m_desc(desc)
{
    QHWI_ASSERT(s_ANativeWindow);
    m_surface.width = ANativeWindow_getWidth(s_ANativeWindow);
    m_surface.height = ANativeWindow_getHeight(s_ANativeWindow);
    m_surface.aNativeWindow = s_ANativeWindow;
    QHWI_LOGI("Android window: %d x %d", m_surface.width, m_surface.height);
}

void qhwi::ext::SetANativeWindow(ANativeWindow* window)
{
    s_ANativeWindow = window;
}
