#
#
#  compileShaders.py
#  QHWI
#
#  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#  SPDX-License-Identifier: BSD-3-Clause-Clear
#

import os, platform, argparse, subprocess, sys, shutil
from typing import NamedTuple

sys.path.append('../../../common/scripts')
from shaderUtil import *

def main():
    init()
    compileShaders('gradient', [ vertexShaderDesc('.hlsl'), pixelShaderDesc('.hlsl') ])
    compileShaders('vk_custom_resolve', [ vertexShaderDesc('.hlsl'), pixelShaderDesc('.hlsl') ])
    compileShaders('tonemap', [ vertexShaderDesc('.hlsl'), pixelShaderDesc('.hlsl') ])
main()