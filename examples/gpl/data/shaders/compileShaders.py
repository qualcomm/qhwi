#
#
#  compileShaders.py
#  QHWI
#
#  Copyright (C) 2023-2024 Qualcomm Technologies, Inc.
#  SPDX-License-Identifier: BSD-3-Clause-Clear
#

import os, platform, argparse, subprocess, sys, shutil, math
from typing import NamedTuple

sys.path.append('../../../common/scripts')
from shaderUtil import *

def main():
    init()
    compileShaders( "deadVaryings", [vertexShaderDesc(suffix = '.hlsl', spirvFlags="")], "deadVaryings")
    compileShaders( "deadVaryings", [pixelShaderDesc( suffix = '.hlsl', spirvFlags="")], "deadVaryings")
    compileShaders( "heavyVec4",    [vertexShaderDesc(suffix = '.hlsl', spirvFlags="")], "heavyVec4")
    compileShaders( "heavyVec4",    [pixelShaderDesc( suffix = '.hlsl', spirvFlags="")], "heavyVec4")
    compileShaders( "largeUbo",     [vertexShaderDesc(suffix = '.hlsl', spirvFlags="")], "largeUbo")
    compileShaders( "largeUbo",     [pixelShaderDesc( suffix = '.hlsl', spirvFlags="")], "largeUbo")
    for index in range(0,1024):
        if index > 0:
            bits = int(max(10, math.log(index, 2)+1))
        else:
            bits = 10
        out = [1 if index & (1 << (bits-1-n)) else 0 for n in range(bits)]
        out = out[::-1]

        spirvFlagsStr = ""
        for bit in range(0,10):
            if out[bit] > 0 :
                spirvFlagsStr+='-DPERM_' + str(bit) + ' '

        print("index=" + str(index) + " spirvFlagsStr="+spirvFlagsStr)

        compileShaders('permutations', [vertexShaderDesc(suffix = '.hlsl', spirvFlags=spirvFlagsStr)], "permutations"+str(index))
        compileShaders('permutations', [pixelShaderDesc(suffix = '.hlsl', spirvFlags=spirvFlagsStr)], "permutations"+str(index))


main()