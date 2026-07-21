//
//
//  qhwi_glfw.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifndef __QHWI_GLFW_H__
#define __QHWI_GLFW_H__
#include <qhwi/qhwi.h>
#include <GLFW/glfw3.h>

namespace qhwi::ext
{
class GlfwContext
{
public:
    static inline GlfwContext* getInstance() { return &m_instance; }

    void initOnce();
    static void onError(int errorCode, const char* msg);
    ~GlfwContext();

private:
    int m_state = 0;
    static GlfwContext m_instance;
};

class GlfwWindow : public Window
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

    GlfwWindow(const Description& desc);
    virtual ~GlfwWindow();

    Window::Description* desc() override { return &m_desc.base; }

    const Window::Description* desc() const override { return &m_desc.base; }

    void pollEvents() override { glfwPollEvents(); }

    bool shouldClose() override { return glfwWindowShouldClose(m_window); }

private:
    void initSurface();
    void onWindowResize(int w, int h);
    GLFWwindow* m_window = nullptr;
    Description m_desc;
};
} // namespace qhwi::ext

#endif
