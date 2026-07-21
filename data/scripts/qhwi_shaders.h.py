#
#
#  qhwi_shaders.h.py
#  QHWI
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

# To verify hexdumps, run the following command
# //> hexdump shader.dxil -v -e '64/4 "0x%08x, " "\n"' > shader.dxil.hex
# //> hexdump shader.spv -v -e '64/4 "0x%08x, " "\n"'  > shader.spv.hex
# //> hexdump shader.metallib -v -e '64/4 "0x%08x, " "\n"'  > shader.metallib.hex
import os, argparse, struct

def posixPath(p):
        return p.replace(os.sep, '/')

class Shader:
    def __init__(self, sourceDir, fileName, shaderExt):
        self.shaderExt = shaderExt
        self.sourceDir = sourceDir
        self.fileName  = fileName
        self.fullPath  = sourceDir + '/' + fileName + '.' + shaderExt
        self.shaderHexdump = ''
        self.sizeBytes = 0

    # Read file, and generate a hexdump
    def process(self):
        if os.path.isfile(self.fullPath):
            self.sizeBytes = os.path.getsize(self.fullPath)

            with open(self.fullPath, "rb") as file:
                hexStrList  = []
                numBytes    = 4
                bitsPerByte = 8
                dword       = 0

                for i in range(0, self.sizeBytes):
                    shift = (i % numBytes) * bitsPerByte
                    dword = dword | struct.unpack('=B', file.read(1))[0] << shift
                    if (i % numBytes) == (numBytes - 1):
                        hexStrList.append(hex(dword))
                        dword = 0

                if (self.sizeBytes % numBytes) != 0:
                    hexStrList.append(hex(dword))

                print("{} found, generating hexdump".format(self.fullPath))
                self.shaderHexdump = ",".join(hexStrList)
        else:
            print("{} not found".format(self.fullPath))
            self.shaderHexdump = '0x0'

# Parse arguments
ArgumentParser = argparse.ArgumentParser()
ArgumentParser.add_argument('--outputDir', required=True, default=posixPath(os.getcwd()))
ArgumentParser.add_argument('--sourceDir', default=posixPath(os.getcwd()))
opts = ArgumentParser.parse_args()
opts.outputDir = os.path.normpath(opts.outputDir)

# Shader variants
multiPassVariants = [f"generateMipmapsMultiPassFloat{n}_CSMain" for n in range(1, 5)]
singlePassVariants = [f"generateMipmapsSinglePassFloat{n}_CSMain" for n in range(1, 5)]
outHeaderFilename = "qhwi_shaders.h"

# Mapping from file extension to suffix
suffixMap = {
    "dxil": "DXIL",
    "spv": "SPIRV",
    "metallib": "MTL"
}

shaderDict = {}

for variant in multiPassVariants + singlePassVariants:
    shaderDict[variant] = {}
    for ext, suffix in suffixMap.items():
        shader = Shader(opts.sourceDir, variant, ext)
        shader.process()
        shaderDict[variant][suffix] = shader

# Header generation
qhwi_shaders_h = """\
//
//  qhwi_shaders.h
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//
#ifndef __QHWI_SHADERS_H__
#define __QHWI_SHADERS_H__

namespace qhwi
{
"""

for variant in shaderDict:
    for suffix in shaderDict[variant]:
        arrayName = f"{variant}_{suffix}"
        qhwi_shaders_h += f"    constexpr uint32_t {arrayName}[] =\n    {{\n        {shaderDict[variant][suffix].shaderHexdump}\n    }};\n\n"

qhwi_shaders_h += """\
    struct MipmapShaderInfo
    {
        const uint32_t* shaderBlob;
        size_t          blobSize;
    };
"""

for suffix in ["DXIL", "SPIRV", "MTL"]:
    qhwi_shaders_h += f"\n    MipmapShaderInfo MipmapShader{suffix}[] =\n    {{\n"
    for variant in singlePassVariants + multiPassVariants:
        arrayName = f"{variant}_{suffix}"
        qhwi_shaders_h += f"        {{ {arrayName}, sizeof({arrayName}) }},\n"
    qhwi_shaders_h += "    };\n"

qhwi_shaders_h += "\n} // namespace qhwi\n#endif // __QHWI_SHADERS_H__\n"

# Write to file
outputPath = os.path.normpath(os.path.join(opts.outputDir, outHeaderFilename))

import time

retries = 10
delay = 0.5
success = False

# Sometimes the file can be locked by other process
for attempt in range(retries):
    try:
        with open(outputPath, "w") as f:
            print("Written {} bytes to {}".format(f.write(qhwi_shaders_h), outputPath))
            success = True
            f.flush()
            os.fsync(f.fileno())
            break
    except OSError as e:
        print(f"Attempt {attempt+1}: Failed to open {outputPath} for writing: {e}")
        time.sleep(delay)

if not success:
    print("Giving up on writing file.")


