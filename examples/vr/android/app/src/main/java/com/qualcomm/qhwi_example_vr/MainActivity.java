package com.qualcomm.qhwi_example_vr;
public class MainActivity extends android.app.NativeActivity
{
    static
    {
        System.loadLibrary("vrapi");
        System.loadLibrary("qhwi_example_vr");
    }
}