//
//
//  qhwi_enums_from_d3d12.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <qhwi/qhwi_d3d12_enums.h>
#include "qhwi_d3d12_enums_internal.h"

#define CASE(p0, p1)                                                                                                                                                               \
    case p1:                                                                                                                                                                       \
        return p0;                                                                                                                                                                 \
        break

qhwi::Format qhwi::qhwi(DXGI_FORMAT format)
{
    switch (format)
    {
        QHWI_DXGI_FORMAT_MAP();
    default:
        return Format(0);
    };
}
