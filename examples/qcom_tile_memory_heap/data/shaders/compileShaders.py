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
    compileShaders('testUtilDrawImage')
    compileShaders('blit', [ computeShaderDesc() ])
main()