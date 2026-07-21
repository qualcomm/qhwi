//
//
//  qhwi_pix.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifndef __QHWI_PIX_H__
#define __QHWI_PIX_H__

#include "qhwi.h"

#include <pix3.h>

#include <string>

namespace qhwi::ext
{
class PixGpuCapture : public GpuCapture
{
public:
    PixGpuCapture(const char* path);

    void startFrameCapture() override;
    void endFrameCapture() override;

    void setCaptureOutputPath(const char* path) override;

    // Captures next X amount of frames, equivalent to pressing the print-screen button in PIX
    void captureNextFrames(uint32_t frameCount = 1);

private:
    std::wstring m_captureOutputPath;
    PIXCaptureParameters m_captureParameters;
};
} // namespace qhwi::ext

#endif