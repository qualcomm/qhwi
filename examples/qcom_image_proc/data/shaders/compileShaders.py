#
#
#  compileShaders.py
#  QHWI
#
#  Copyright (C) 2023 Qualcomm Technologies, Inc.
#  SPDX-License-Identifier: BSD-3-Clause-Clear
#

import os, platform, argparse, subprocess, sys, shutil
from typing import NamedTuple

sys.path.append('../../../common/scripts')
from shaderUtil import *

def main():
    init()
    compileShaders('downscale')
    compileShadersSpv('downscale_qcom', [
        vertexShaderDesc(),
        pixelShaderDesc(spirvFlags='-fspv-target-env=vulkan1.1spirv1.4')
    ])
    compileGlslShadersSpv('downscale_qcom')
    compileShaders('drawImage', [ vertexShaderDesc('.hlsl'), pixelShaderDesc('.hlsl') ])

main()