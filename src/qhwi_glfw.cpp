//
//
//  qhwi_glfw.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <qhwi/qhwi_glfw.h>
#if defined(QHWI_PLATFORM_LINUX)
#    include <qhwi/qhwi_vk_config.h>
#endif
#if defined(QHWI_PLATFORM_WINDOWS)
#    define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(QHWI_PLATFORM_LINUX)
#    define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

qhwi::ext::GlfwContext qhwi::ext::GlfwContext::m_instance;

void qhwi::ext::GlfwContext::initOnce()
{
    if (m_state == 0)
    {
        glfwSetErrorCallback(GlfwContext::onError);
        if (!glfwInit())
        {
            QHWI_FATAL("glfw initialization failed");
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        m_state = 1;
    }
}

void qhwi::ext::GlfwContext::onError(int errorCode, const char* msg)
{
    QHWI_ERR("errorCode: %d, message: %s", errorCode, msg);
}

qhwi::ext::GlfwContext::~GlfwContext()
{
    if (m_state == 1)
    {
        glfwTerminate();
    }
}

qhwi::ext::GlfwWindow::GlfwWindow(const Description& desc)
    : m_desc(desc)
{
    GlfwContext::getInstance()->initOnce();
    auto monitor = glfwGetPrimaryMonitor();
    auto videoMode = glfwGetVideoMode(monitor);
    int width = m_desc.base.width;
    int height = m_desc.base.height;
    if (width == DisplayWidth)
    {
        width = videoMode->width;
    }
    if (height == DisplayHeight)
    {
        height = videoMode->height;
    }
    m_window = glfwCreateWindow(width, height, m_desc.base.name, nullptr, nullptr);
    initSurface();
    glfwSetWindowUserPointer(m_window, this);
    glfwSetWindowSizeCallback(m_window,
                              [](GLFWwindow* window, int w, int h)
                              {
                                  auto* thiz = (qhwi::ext::GlfwWindow*)glfwGetWindowUserPointer(window);
                                  thiz->onWindowResize(w, h);
                              });
}

void qhwi::ext::GlfwWindow::onWindowResize(int w, int h)
{
    initSurface();
    if (m_windowResizeCb)
    {
        (*m_windowResizeCb)(m_userdata, uint32_t(w), uint32_t(h));
    }
}

void qhwi::ext::GlfwWindow::initSurface()
{
    glfwGetFramebufferSize(m_window, &m_surface.width, &m_surface.height);
#if defined(QHWI_PLATFORM_WINDOWS)
    m_surface.hwnd = glfwGetWin32Window(m_window);
#elif defined(QHWI_PLATFORM_LINUX)
#    if defined(VK_USE_PLATFORM_XLIB_KHR)
    m_surface.window = glfwGetX11Window(m_window);
    m_surface.display = glfwGetX11Display();
#    elif defined(VK_USE_PLATFORM_XCB_KHR)
    m_surface.drawable = glfwGetX11Window(m_window);
    m_surface.connection = (xcb_connection_t*)glfwGetX11Display();
#    elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    m_surface.wlDisplay = glfwGetWaylandDisplay();
    m_surface.wlSurface = glfwGetWaylandWindow(m_window);
#    else
#        error "GLFW backend requires Xlib, XCB, or Wayland WSI"
#    endif
#endif
}

qhwi::ext::GlfwWindow::~GlfwWindow()
{
    glfwDestroyWindow(m_window);
}
