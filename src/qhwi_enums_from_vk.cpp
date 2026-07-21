//
//
//  qhwi_enums_from_vk.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <qhwi/qhwi_vk_enums.h>
#include "qhwi_vk_enums_internal.h"

#define CASE(p0, p1)                                                                                                                                                               \
    case p1:                                                                                                                                                                       \
        return p0;                                                                                                                                                                 \
        break

qhwi::Format qhwi::qhwi(vk::Format format)
{
    switch (format)
    {
        QHWI_VK_FORMAT_MAP();
    default:
        return Format(0);
    };
}
