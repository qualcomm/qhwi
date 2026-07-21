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
    os.chdir('../../../../test/data/shaders')
    compileShaders('testUtilDrawColor1')

main()