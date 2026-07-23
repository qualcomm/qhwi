//
//
//  MainActivity.java
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

package com.qualcomm.qhwi_example_vr;
public class MainActivity extends android.app.NativeActivity
{
    static
    {
        System.loadLibrary("vrapi");
        System.loadLibrary("qhwi_example_vr");
    }
}