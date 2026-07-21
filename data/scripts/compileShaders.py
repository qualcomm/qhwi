#
#  compileShaders.py
#  QHWI
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import os, platform, argparse, subprocess, sys, shutil
from typing import NamedTuple

sys.path.append('../../examples/common/scripts')
import shaderUtil
from shaderUtil import *

def main():
    init()
    Opts = shaderUtil.Opts

    shaderSourceNameMultipass : str = "generateMipmapsMultiPassFloatN"
    shaderSourceNameSinglepass : str = "generateMipmapsSinglePassFloatN"

    # Compile shaders with NUM_CHANNELS = 1 to 4
    for numChannels in [1, 2, 3, 4]:

        macro = f"NUM_CHANNELS={numChannels}"
        # Create a ShaderDesc with macro
        desc = computeShaderDesc()._replace(macros=[macro])

        outputNameMultipass = f"generateMipmapsMultiPassFloat{numChannels}"
        outputNameSinglepass = f"generateMipmapsSinglePassFloat{numChannels}"

        compileShaders(shaderSourceNameMultipass, [desc], outputName=outputNameMultipass)
        compileShaders(shaderSourceNameSinglepass, [desc], outputName=outputNameSinglepass)

main()
