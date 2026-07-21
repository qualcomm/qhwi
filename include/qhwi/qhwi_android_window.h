//
//
//  qhwi_android_window.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifndef __QHWI_ANDROID_WINDOW_H__
#define __QHWI_ANDROID_WINDOW_H__
#include <android/native_window.h>
#include "qhwi.h"

namespace qhwi::ext
{
class AndroidWindow : public Window
{
public:
    struct Description
    {
        Description(const Window::Description& desc)
            : base(desc)
        {
        }

        Window::Description base;
    };

    AndroidWindow(const Description& desc);

    Window::Description* desc() override { return &m_desc.base; }

    const Window::Description* desc() const override { return &m_desc.base; }

    bool shouldClose() override { return false; }

private:
    Description m_desc;
};

extern void SetANativeWindow(ANativeWindow* window);
} // namespace qhwi::ext

#endif