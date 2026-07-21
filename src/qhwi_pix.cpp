//
//
//  qhwi_pix.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <qhwi/qhwi_pix.h>

#include <pix3.h>

#include <qre/text/unicode.h>

qhwi::ext::PixGpuCapture::PixGpuCapture(const char* path)
{
    // Find WinPixGpuCapturer.dll, this must be called before calling any D3DAPIs
    PIXLoadLatestWinPixGpuCapturerLibrary();

    setCaptureOutputPath(path);
}

void qhwi::ext::PixGpuCapture::setCaptureOutputPath(const char* path)
{
    // Convert path to wstring to be able to use with PIX API.
    size_t wcCount = qre::CharToWideChar(nullptr, 0, path);
    m_captureOutputPath = std::wstring(wcCount, L'0');
    qre::CharToWideChar(m_captureOutputPath.data(), m_captureOutputPath.size() + 1, path);

    m_captureParameters.GpuCaptureParameters.FileName = m_captureOutputPath.c_str();
}

void qhwi::ext::PixGpuCapture::startFrameCapture()
{
    PIXBeginCapture(PIX_CAPTURE_GPU, &m_captureParameters);
}

void qhwi::ext::PixGpuCapture::endFrameCapture()
{
    PIXEndCapture(false); // `discard` parameter is only used on XBOX
}

void qhwi::ext::PixGpuCapture::captureNextFrames(uint32_t frameCount)
{
    PIXGpuCaptureNextFrames(m_captureOutputPath.c_str(), frameCount);
}
