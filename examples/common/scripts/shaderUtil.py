#
#
#  shaderUtil.py
#  QHWI
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import os, platform, argparse, subprocess, sys, shutil
from typing import NamedTuple
from enum import Enum
import re
import shlex

# Global Variables

Opts = None
SpirvFlags = None
VulkanSdk = None
OutputDir = None
MinVersion = (3, 9, 6)

def posixPath(p):
    return p.replace(os.sep, '/')

def runCommand(args):
    print(args)
    args = shlex.split(args)
    proc = subprocess.Popen(args, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    out,err = proc.communicate()
    code = proc.returncode
    print('{0}{1}{2}'.format(out.decode('utf-8'), '' if code == 0 else 'ERROR: ', err.decode('utf-8')))

def copyFile(src, dst):
    print(f'copying {src} to {dst}')
    shutil.copyfile(src, dst)

class ShaderDesc(NamedTuple):
    suffix: str = '.hlsl'
    entryPoint: str = ''
    profile: str = ''
    spirvFlags: str = ''
    macros: list = []

def makeShaderDesc(suffix, entryPoint, profile, spirvFlags):
    return ShaderDesc(suffix, entryPoint, profile, spirvFlags)

def vertexShaderDesc(suffix: str = '_vs.hlsl', entryPoint: str = 'VSMain', profile: str = 'vs_6_5', spirvFlags: str = '') -> ShaderDesc:
    return makeShaderDesc(suffix, entryPoint, profile, spirvFlags)
def multiViewVertexShaderDesc(suffix: str = '_vs.hlsl', entryPoint: str = 'VSMain', profile: str = 'vs_6_5', spirvFlags: str = '') -> ShaderDesc:
    return makeShaderDesc(suffix, entryPoint, profile, spirvFlags)
def pixelShaderDesc(suffix: str = '_ps.hlsl', entryPoint: str = 'PSMain', profile: str = 'ps_6_5', spirvFlags: str = '') -> ShaderDesc:
    return makeShaderDesc(suffix, entryPoint, profile, spirvFlags)
def computeShaderDesc(suffix: str = '_cs.hlsl', entryPoint: str = 'CSMain', profile: str = 'cs_6_5', spirvFlags: str = '') -> ShaderDesc:
    return makeShaderDesc(suffix, entryPoint, profile, spirvFlags)
def meshShaderDesc(suffix: str = '_ms.hlsl', entryPoint: str = 'MSMain', profile: str = 'ms_6_5',
        spirvFlags: str = ' -fspv-extension=SPV_EXT_mesh_shader -fspv-target-env=vulkan1.1spirv1.4') -> ShaderDesc:
    return makeShaderDesc(suffix, entryPoint, profile, spirvFlags)
def amplificationShaderDesc(suffix: str = '_as.hlsl', entryPoint: str = 'ASMain', profile: str = 'as_6_5',
        spirvFlags: str = ' -fspv-extension=SPV_EXT_mesh_shader -fspv-target-env=vulkan1.1spirv1.4') -> ShaderDesc:
    return makeShaderDesc(suffix, entryPoint, profile, spirvFlags)

def glslVertexShaderDesc(suffix: str = '_vs.glsl', entryPoint: str = 'main') -> ShaderDesc:
    return makeShaderDesc(suffix, entryPoint, '', '-fshader-stage=vert')
def glslPixelShaderDesc(suffix: str = '_ps.glsl', entryPoint: str = 'main') -> ShaderDesc:
    return makeShaderDesc(suffix, entryPoint, '', '-fshader-stage=frag')

def compileShaderDxil(name, desc: ShaderDesc, outputName):
    if '.hlsl' in desc.suffix:
        macroFlags = ' '.join([f'-D{macro}' for macro in desc.macros]) if desc.macros else ''
        runCommand(f'{Opts.dxcDxilPath}  -E {desc.entryPoint} {macroFlags} -T {desc.profile} -O3 -Zi -Qembed_debug -Fo {OutputDir}/{outputName}_{desc.entryPoint}.dxil {name}{desc.suffix}')

def getGlslShaderStage(spirvFlags):
    m = re.search(r'shader-stage=([a-z]+)', spirvFlags)
    return m.group(1)

def compileShaderSpv(name, desc: ShaderDesc, outputName):
    macroFlags = ' '.join([f'-D{macro}' for macro in desc.macros]) if desc.macros else ''
    if '.hlsl' in desc.suffix:
        runCommand(f'{Opts.dxcSpirvPath} -E {desc.entryPoint} {macroFlags} -T {desc.profile} -O3 -Zi -Qembed_debug -Fo {OutputDir}/{outputName}_{desc.entryPoint}.spv  {SpirvFlags} {desc.spirvFlags} {name}{desc.suffix}')
    elif '.glsl' in desc.suffix:
        shaderStage = getGlslShaderStage(desc.spirvFlags)
        runCommand(f'{Opts.glslcPath} -fentry-point={desc.entryPoint} {macroFlags} {desc.spirvFlags} -o {OutputDir}/{outputName}_glsl_{shaderStage}_{desc.entryPoint}.spv {name}{desc.suffix}')

def compileShadersDxil(name, descs = None, outputName=None):
    if descs is None:
        descs = [ vertexShaderDesc(), pixelShaderDesc() ]

    for desc in descs:
        if outputName is None:
           outputName = name
        compileShaderDxil(name, desc, outputName)

def compileShadersSpv(name, descs = None, outputName=None):
    if descs is None:
        descs = [ vertexShaderDesc(), pixelShaderDesc() ]

    for desc in descs:
        if outputName is None:
           outputName = name
        compileShaderSpv(name, desc, outputName)

def compileGlslShadersSpv(name, descs = None,  outputName=None):
    if descs is None:
        descs = [ glslVertexShaderDesc(), glslPixelShaderDesc() ]
    for desc in descs:
        if outputName is None:
           outputName = name
        compileShaderSpv(name, desc, outputName)

def compileShaders(name, descs = None,  outputName=None):
    if Opts.enableD3d12Backend:
        compileShadersDxil(name, descs, outputName)
    if Opts.enableVulkanBackend:
        compileShadersSpv(name, descs, outputName)

def init():
    global Opts, SpirvFlags, VulkanSdk, OutputDir
    if sys.version_info < MinVersion:
        print(f'WARNING: version: {sys.version_info} < minVersion: {MinVersion}')
    sys.argv = [arg for arg in sys.argv if arg != '']
    print('current dir: ', os.getcwd())
    print('command: ', ' '.join(sys.argv))
    print('path: ', sys.path)

    ArgumentParser = argparse.ArgumentParser()
    ArgumentParser.add_argument('--systemName', default = platform.system())
    ArgumentParser.add_argument('--enableVulkanBackend', action='store_true')
    ArgumentParser.add_argument('--enableD3d12Backend', action='store_true')
    ArgumentParser.add_argument('--enableSER', action='store_true', help='Enable Shader Execution Reordering (SER) shader compilation')
    ArgumentParser.add_argument('--dxcSpirvPath')
    ArgumentParser.add_argument('--dxcDxilPath')
    ArgumentParser.add_argument('--glslcPath')
    ArgumentParser.add_argument('--outputDir', required = True)
    ArgumentParser.add_argument('--sourceDir', default = posixPath(os.getcwd()))
    Opts = ArgumentParser.parse_args()

    SpirvFlags = ' -spirv -DSPIRV -fspv-preserve-interface -fspv-preserve-bindings \
    -fvk-u-shift 384 0 -fvk-t-shift 0 0 -fvk-b-shift 256 0 -fvk-s-shift 128 0 -fvk-u-shift 384 1 \
    -fvk-t-shift 0 1 -fvk-b-shift 256 1 -fvk-s-shift 128 1'

    VulkanSdk = posixPath((os.environ.get('VULKAN_SDK', '') if Opts.enableVulkanBackend else '').strip())

    if Opts.enableVulkanBackend and not VulkanSdk:
        print('WARNING: --enableVulkanBackend is set but VULKAN_SDK environment variable is not set. Vulkan tools will be looked up on PATH.')

    if Opts.systemName == 'Windows':
        dxcDefaultPath = f'{VulkanSdk}/Bin/dxc.exe' if (Opts.enableVulkanBackend and VulkanSdk) else 'dxc.exe'
        glslcDefaultPath = f'{VulkanSdk}/Bin/glslc.exe' if (Opts.enableVulkanBackend and VulkanSdk) else 'glslc.exe'
    else:
        dxcDefaultPath = 'dxc'
        glslcDefaultPath = 'glslc'

    if not Opts.dxcSpirvPath:
        Opts.dxcSpirvPath = dxcDefaultPath
    if not Opts.dxcDxilPath:
        Opts.dxcDxilPath = dxcDefaultPath
    if not Opts.glslcPath:
        Opts.glslcPath = glslcDefaultPath

    if Opts.systemName == 'Android':
        OutputDir = f'{Opts.sourceDir}/../../android/app/src/main/assets/shaders'
    else:
        OutputDir = f'{Opts.outputDir}'

    if not os.path.exists(OutputDir):
        os.makedirs(OutputDir)
