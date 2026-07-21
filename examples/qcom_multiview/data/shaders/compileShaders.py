#
#
#  compileShaders.py
#  QHWI
#
#  Copyright (C) 2023-2024 Qualcomm Technologies, Inc.
#  SPDX-License-Identifier: BSD-3-Clause-Clear
#

import os, platform, argparse, subprocess, sys, shutil
from typing import NamedTuple

sys.path.append('../../../common/scripts')
from shaderUtil import *

def main():
    init()
    compileShaders('qcom_multiview', [ multiViewVertexShaderDesc('.hlsl'), pixelShaderDesc('.hlsl') ])
    compileShaders('multiview', [ multiViewVertexShaderDesc('.hlsl'), pixelShaderDesc('.hlsl') ])
    compileShaders('testUtilDrawImage')
main()