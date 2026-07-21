//
//
//  qhwi_d3d_internal_extensions.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "qhwi_d3d_internal_extensions.h"
using namespace qhwi_d3d_internal_extensions;

#define WRITE_ENUM_VALUE(nameArray, enumId) GenericWrite("\t%s,\n", enum##nameArray[enumId]);

#define WRITE_ENUM_VALUE_NOTAB(nameArray, enumId) GenericWrite("%s,\n", enum##nameArray[enumId]);

#define WRITE_UINT_DEC_VALUE(var) GenericWrite("\t%u,\t\t//%s\n", var, #var);

#define WRITE_UINT_HEX_VALUE(var) GenericWrite("\t0x%X,\t\t//%s\n", var, #var);

#define WRITE_PTR_VALUE(var, pArgs, ...)                                                                                                                                           \
    GenericWrite("\t%s%s_%d,\t\t", pArgs, __VA_ARGS__);                                                                                                                            \
    GenericWrite("//%s\n", #var);

#define WRITE_PTR_VALUE_NOID(var, pArgs, ...)                                                                                                                                      \
    GenericWrite("\t%s%s,\t\t", pArgs, __VA_ARGS__);                                                                                                                               \
    GenericWrite("//%s\n", #var);

#define WRITE_PTR_VALUE_NOID_NOTAB(var, pArgs, ...)                                                                                                                                \
    GenericWrite("%s%s,\t\t", pArgs, __VA_ARGS__);                                                                                                                                 \
    GenericWrite("//%s\n", #var);

#define WRITE_DESIGNATED_ENUM_VALUE(variableName, table, value)                                                                                                                    \
    GenericWrite("\t.%s = ", #variableName);                                                                                                                                       \
    WRITE_ENUM_VALUE_NOTAB(table, value);

#define WRITE_DESIGNATED_PTR_VALUE_NOID(variableName, desc_1_1, rootSignature1_1VariableName)                                                                                      \
    GenericWrite("\t.%s = ", #variableName);                                                                                                                                       \
    WRITE_PTR_VALUE_NOID_NOTAB(desc_1_1, rootSignature1_1VariableName, "");

int ExportHeader::GenericWrite(const char* pFmt, ...)
{
    QRE_ASSERT(m_fileHandle != nullptr);

    int ret = -1;

    if (m_fileHandle != nullptr)
    {
        va_list args;
        va_start(args, pFmt);
        ret = vfprintf(m_fileHandle, pFmt, args);
        va_end(args);
    }

    return ret;
}

void ExportHeader::ExportDxil(const void* shader, unsigned int len)
{
    if (m_exportOption == true)
    {
        QRE_ASSERT((shader != nullptr) && (len != 0));

        if ((shader == nullptr) || (len == 0))
        {
            return;
        }

        /*Write shader name*/
        GenericWrite("\nconst CHAR* shaderName = \"%s\";\n", m_shadername);

        /*Write shader dxil blob*/
        GenericWrite("\n/*DXIL - %s Shader*/", m_shadername);

        GenericWrite("\nconst UINT shaderDxilBlob[] =\n{\n");
        const unsigned int* shaderUint = reinterpret_cast<const unsigned int*>(shader);
        unsigned int lenUint = len / sizeof(unsigned int);
        unsigned int lenRemainderUint = len % sizeof(unsigned int);

        for (unsigned int i = 0; i < lenUint; i++)
        {
            fprintf_s(m_fileHandle, "\t0x%08X,\n", shaderUint[i]);
        }

        if (lenRemainderUint)
        {
            unsigned int remainderValue = 0x0;
            const unsigned char* shaderRemainder = reinterpret_cast<const unsigned char*>(shader) + (len - lenRemainderUint);
            for (unsigned int i = 0; i < lenRemainderUint; i++)
            {
                remainderValue |= (shaderRemainder[i] << (i * 8));
            }
            fprintf_s(m_fileHandle, "\t0x%08X,\n", remainderValue);
        }

        GenericWrite("};\n");
    }
}

void ExportHeader::WriteFileHeader()
{
    if (m_exportOption == true)
    {
        QRE_ASSERT(m_guardname != nullptr);
        GenericWrite("\n#ifndef %s\n", m_guardname);
        GenericWrite("#define %s\n", m_guardname);
        GenericWrite("\n#include <d3d12.h>\n");
    }
}

void ExportHeader::WriteFileFooter()
{
    if (m_exportOption == true)
    {
        QRE_ASSERT(m_guardname != nullptr);
        GenericWrite("\n#endif // !%s\n", m_guardname);
    }
}

void ExportHeader::FileOpen(const char* shaderName)
{
    const int GenericFilePathLen = static_cast<int>(strlen(GenericFilePath));
    const int GenericFilenamePrefixLen = static_cast<int>(strlen(GenericFilenamePrefix));
    const int FixLen = 3; //.h and null terminator

    m_shadernameLen = static_cast<int>(strlen(shaderName)) + 1;
    m_filenameLen = GenericFilenamePrefixLen + m_shadernameLen + FixLen + 1;
    m_fullFilenameLen = GenericFilePathLen + m_filenameLen + 1;
    m_guardnameLen = m_filenameLen + 3; // 2 _s and 1 null terminator.

    m_shadername = new char[m_shadernameLen];
    m_filename = new char[m_filenameLen];
    m_fullFilename = new char[m_fullFilenameLen];
    m_guardname = new char[m_guardnameLen];

    memset(m_shadername, 0, m_shadernameLen);
    memset(m_filename, 0, m_filenameLen);
    memset(m_fullFilename, 0, m_fullFilenameLen);
    memset(m_guardname, 0, m_guardnameLen);

    sprintf_s(m_shadername, m_shadernameLen, "%s", shaderName);
    sprintf_s(m_filename, m_filenameLen, "%s%s.h", GenericFilenamePrefix, shaderName);
    sprintf_s(m_fullFilename, m_fullFilenameLen, "%s%s", GenericFilePath, m_filename);
    sprintf_s(m_guardname, m_guardnameLen, "_%s_", m_filename);

    for (int i = 0; i < m_guardnameLen; i++)
    {
        if (m_guardname[i] == '.')
        {
            m_guardname[i] = '_';
        }
        m_guardname[i] = static_cast<char>(toupper(static_cast<char>(m_guardname[i])));
    }

    /*Open a file to export the information*/
    errno_t e = fopen_s(&m_fileHandle, m_fullFilename, "w");
    QRE_ASSERT(e == 0);
}

void ExportHeader::FileClose()
{
    /*Close the file*/
    int r = fclose(m_fileHandle);
    QRE_ASSERT(r == 0);

    m_shadernameLen = 0;
    m_filenameLen = 0;
    m_fullFilenameLen = 0;
    m_guardnameLen = 0;

    delete[] m_shadername;
    delete[] m_filename;
    delete[] m_fullFilename;
    delete[] m_guardname;
}

void ExportHeader::ExportDescriptorRange1(unsigned int bindingIndex, D3D12_DESCRIPTOR_RANGE1 r)
{
    if (m_exportOption == true)
    {
        D3D12_DESCRIPTOR_RANGE_TYPE rangeType = r.RangeType;
        unsigned int numDescriptors = r.NumDescriptors;
        unsigned int baseShaderRegister = r.BaseShaderRegister;
        unsigned int registerSpace = r.RegisterSpace;
        D3D12_DESCRIPTOR_RANGE_FLAGS flags = r.Flags;
        unsigned int offsetInDescriptorsFromTableStart = r.OffsetInDescriptorsFromTableStart;

        GenericWrite("\nconst %s %s%s_%d[] =\n{\n",
                     d3d12DescriptorRangeTypeName,
                     descriptorRangeVariableName,
                     rangeTypeVariableNameTable[static_cast<unsigned int>(r.RangeType)],
                     bindingIndex);

        WRITE_ENUM_VALUE(D3D12_DESCRIPTOR_RANGE_TYPE, rangeType);
        WRITE_UINT_DEC_VALUE(numDescriptors);
        WRITE_UINT_DEC_VALUE(baseShaderRegister);
        WRITE_UINT_DEC_VALUE(registerSpace);
        WRITE_ENUM_VALUE(D3D12_DESCRIPTOR_RANGE_FLAGS, flags);
        WRITE_UINT_HEX_VALUE(offsetInDescriptorsFromTableStart);

        GenericWrite("};\n");
    }
}

void ExportHeader::ExportRootParamAsDescriptorTable(unsigned int bindingIndex, D3D12_ROOT_PARAMETER1 p)
{
    if (m_exportOption == true)
    {
        const unsigned int numDescriptorRanges = p.DescriptorTable.NumDescriptorRanges;
        const D3D12_DESCRIPTOR_RANGE1* pDescriptorRanges = p.DescriptorTable.pDescriptorRanges;

        /*
        Currently assuming only 1 desc range per desc table as it builds the root signature in function: qhwi::d3dPipelineLayout::Init().
        When multiple ranges are included per desc table, this logic needs to be changed so that pDescriptorRanges points to an array of desc ranges.
        */
        unsigned int rangeType = static_cast<unsigned int>(pDescriptorRanges->RangeType);

        GenericWrite("\nconst %s %s%s_%d =\n{\n", d3d12DescriptorTableTypeName, descriptorTableVariableName, rangeTypeVariableNameTable[rangeType], bindingIndex);
        WRITE_UINT_DEC_VALUE(numDescriptorRanges);
        WRITE_PTR_VALUE(pDescriptorRanges, descriptorRangeVariableName, rangeTypeVariableNameTable[rangeType], bindingIndex);
        GenericWrite("};\n");

        GenericWrite("\nconst %s %sDescTable_%d =\n{\n", d3d12DescriptorParamTypeName, rootParamVariableName, bindingIndex);
        WRITE_ENUM_VALUE(D3D12_ROOT_PARAMETER_TYPE, D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE);
        WRITE_PTR_VALUE(p.DescriptorTable, descriptorTableVariableName, rangeTypeVariableNameTable[rangeType], bindingIndex);
        WRITE_ENUM_VALUE(D3D12_SHADER_VISIBILITY, p.ShaderVisibility);
        GenericWrite("};\n");
    }
}

void ExportHeader::ExportRootParamAsConstants(unsigned int bindingIndex, D3D12_ROOT_PARAMETER1 p)
{
    if (m_exportOption == true)
    {
        auto num32BitValues = p.Constants.Num32BitValues;
        auto registerSpace = p.Constants.RegisterSpace;
        auto shaderRegister = p.Constants.ShaderRegister;

        GenericWrite("\nconst %s %s_%d =\n{\n", d3d12RootConstantTypeName, rootConstantVariableName, bindingIndex);
        WRITE_UINT_DEC_VALUE(shaderRegister);
        WRITE_UINT_DEC_VALUE(registerSpace);
        WRITE_UINT_DEC_VALUE(num32BitValues);
        GenericWrite("};\n");

        GenericWrite("\nconst %s %sConstant_%d =\n{\n", d3d12DescriptorParamTypeName, rootParamVariableName, bindingIndex);
        WRITE_ENUM_VALUE(D3D12_ROOT_PARAMETER_TYPE, D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS);
        GenericWrite("\t{},\n");
        WRITE_ENUM_VALUE(D3D12_SHADER_VISIBILITY, p.ShaderVisibility);
        GenericWrite("};\n");
    }
}

void ExportHeader::ExportRootParamAsConstantBufferView(unsigned int bindingIndex, D3D12_ROOT_PARAMETER1 p)
{
    if (m_exportOption == true)
    {
        auto shaderRegister = p.Descriptor.ShaderRegister;
        auto registerSpace = p.Descriptor.RegisterSpace;
        auto flags = p.Descriptor.Flags;

        GenericWrite("\nconst %s %s_%d =\n{\n", d3d12RootDescriptorTypeName, rootCbvDescriptorVariableName, bindingIndex);
        WRITE_UINT_DEC_VALUE(shaderRegister);
        WRITE_UINT_DEC_VALUE(registerSpace);
        WRITE_ENUM_VALUE(D3D12_ROOT_DESCRIPTOR_FLAGS, flags);
        GenericWrite("};\n");

        GenericWrite("\nconst %s %sCbvDesc_%d =\n{\n", d3d12DescriptorParamTypeName, rootParamVariableName, bindingIndex);
        WRITE_ENUM_VALUE(D3D12_ROOT_PARAMETER_TYPE, D3D12_ROOT_PARAMETER_TYPE_CBV);
        GenericWrite("\t{},\n");
        WRITE_ENUM_VALUE(D3D12_SHADER_VISIBILITY, p.ShaderVisibility);
        GenericWrite("};\n");
    }
}

void ExportHeader::ExportRootParamAsShaderResourceView(unsigned int bindingIndex, D3D12_ROOT_PARAMETER1 p)
{
    if (m_exportOption == true)
    {
        auto shaderRegister = p.Descriptor.ShaderRegister;
        auto registerSpace = p.Descriptor.RegisterSpace;
        auto flags = p.Descriptor.Flags;

        GenericWrite("\nconst %s %s_%d =\n{\n", d3d12RootDescriptorTypeName, rootSrvDescriptorVariableName, bindingIndex);
        WRITE_UINT_DEC_VALUE(shaderRegister);
        WRITE_UINT_DEC_VALUE(registerSpace);
        WRITE_ENUM_VALUE(D3D12_ROOT_DESCRIPTOR_FLAGS, flags);
        GenericWrite("};\n");

        GenericWrite("\nconst %s %sSrvDesc_%d =\n{\n", d3d12DescriptorParamTypeName, rootParamVariableName, bindingIndex);
        WRITE_ENUM_VALUE(D3D12_ROOT_PARAMETER_TYPE, D3D12_ROOT_PARAMETER_TYPE_SRV);
        GenericWrite("\t{},\n");
        WRITE_ENUM_VALUE(D3D12_SHADER_VISIBILITY, p.ShaderVisibility);
        GenericWrite("};\n");
    }
}

void ExportHeader::ExportRootParamAsUnorderedAccessView(unsigned int bindingIndex, D3D12_ROOT_PARAMETER1 p)
{
    if (m_exportOption == true)
    {
        auto shaderRegister = p.Descriptor.ShaderRegister;
        auto registerSpace = p.Descriptor.RegisterSpace;
        auto flags = p.Descriptor.Flags;

        GenericWrite("\nconst %s %s_%d =\n{\n", d3d12RootDescriptorTypeName, rootUavDescriptorVariableName, bindingIndex);
        WRITE_UINT_DEC_VALUE(shaderRegister);
        WRITE_UINT_DEC_VALUE(registerSpace);
        WRITE_ENUM_VALUE(D3D12_ROOT_DESCRIPTOR_FLAGS, flags);
        GenericWrite("};\n");

        GenericWrite("\nconst %s %sUavDesc_%d =\n{\n", d3d12DescriptorParamTypeName, rootParamVariableName, bindingIndex);
        WRITE_ENUM_VALUE(D3D12_ROOT_PARAMETER_TYPE, D3D12_ROOT_PARAMETER_TYPE_UAV);
        GenericWrite("\t{},\n");
        WRITE_ENUM_VALUE(D3D12_SHADER_VISIBILITY, p.ShaderVisibility);
        GenericWrite("};\n");
    }
}

void ExportHeader::ExportRootSignature_1_1(D3D12_VERSIONED_ROOT_SIGNATURE_DESC rs)
{
    if (m_exportOption == true)
    {
        char rootParamsNames[MaxRootParameters][MaxStringSize] = {};
        char rootSrCbUavDescNames[MaxRootParameters][MaxStringSize] = {};
        char rootConstantsNames[MaxRootParameters][MaxStringSize] = {};
        unsigned int numSrCbUavDesc = 0;
        unsigned int numConstants = 0;

        const auto numRootParams = rs.Desc_1_1.NumParameters;
        const auto numStaticSamplers = rs.Desc_1_1.NumStaticSamplers;
        const auto pParameters = rs.Desc_1_1.pParameters;
        //const auto pStaticSamplers = rs.Desc_1_1.pStaticSamplers;
        const auto flags = rs.Desc_1_1.Flags;

        for (unsigned int i = 0; i < numRootParams; i++)
        {
            auto paramType = pParameters[i].ParameterType;
            char* s = nullptr;

            switch (paramType)
            {
            case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
            {
                s = const_cast<char*>("DescTable");
                break;
            }
            case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
            {
                s = const_cast<char*>("Constant");
                break;
            }
            case D3D12_ROOT_PARAMETER_TYPE_CBV:
            {
                s = const_cast<char*>("CbvDesc");
                break;
            }
            case D3D12_ROOT_PARAMETER_TYPE_SRV:
            {
                s = const_cast<char*>("SrvDesc");
                break;
            }
            case D3D12_ROOT_PARAMETER_TYPE_UAV:
            {
                s = const_cast<char*>("UavDesc");
                break;
            }
            default:
                break;
            }

            snprintf(rootParamsNames[i], MaxStringSize, "rootParamType%s_%d", s, i);
            if ((paramType == D3D12_ROOT_PARAMETER_TYPE_CBV) || (paramType == D3D12_ROOT_PARAMETER_TYPE_SRV) || (paramType == D3D12_ROOT_PARAMETER_TYPE_UAV))
            {
                qre::PrintfSafe(rootSrCbUavDescNames[numSrCbUavDesc++], "root%s_%d", s, i);
            }
            else if (paramType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
            {
                qre::PrintfSafe(rootConstantsNames[numConstants++], "root%s_%d", s, i);
            }
        }

        /*Generating the root descriptors table - only SRV, CBV, UAV Descriptors*/
        GenericWrite("\nconst %s %s[] = \n{\n", d3d12RootDescriptorTypeName, srCbUavDescTableVariableName);
        if (numSrCbUavDesc > 0)
        {
            for (unsigned int i = 0; i < numSrCbUavDesc; i++)
            {
                GenericWrite("\t%s,\n", rootSrCbUavDescNames[i]);
            }
        }
        else
        {
            GenericWrite("\tNULL,\n");
        }
        GenericWrite("};\n");

        GenericWrite("\nconst %s %s[] = \n{\n", d3d12RootConstantTypeName, constantsTableVariableName);
        if (numConstants > 0)
        {

            for (unsigned int i = 0; i < numConstants; i++)
            {
                GenericWrite("\t%s,\n", rootConstantsNames[i]);
            }
        }
        else
        {
            GenericWrite("\tNULL,\n");
        }
        GenericWrite("};\n");

        /*Generating the root parameter table*/
        GenericWrite("\nconst %s %s[] = \n{\n", d3d12DescriptorParamTypeName, rootParamTableVariableName);
        for (unsigned int i = 0; i < numRootParams; i++)
        {
            GenericWrite("\t%s,\n", rootParamsNames[i]);
        }
        GenericWrite("};\n");

        /*Generating the root signature 1.1 structure*/
        GenericWrite("\nconst %s %s =\n{\n", d3d12RootSignature1_1TypeName, rootSignature1_1VariableName);
        WRITE_UINT_DEC_VALUE(numRootParams);
        WRITE_PTR_VALUE_NOID(pParameters, rootParamTableVariableName, "");
        WRITE_UINT_DEC_VALUE(numStaticSamplers);
        GenericWrite("\tnullptr, //pStaticSamplers\n");
        WRITE_ENUM_VALUE(D3D12_ROOT_SIGNATURE_FLAGS, flags);
        GenericWrite("};\n");

        /*Generating the versioned root signature desc structure*/
        GenericWrite("\nconst %s %s =\n{\n", d3d12VersionedRSDescTypeName, d3d12VersionedRSDescVariableName);
        WRITE_DESIGNATED_ENUM_VALUE(Version, D3D_ROOT_SIGNATURE_VERSION, D3D_ROOT_SIGNATURE_VERSION_1_1);
        WRITE_DESIGNATED_PTR_VALUE_NOID(Desc_1_1, desc_1_1, rootSignature1_1VariableName);
        GenericWrite("};\n");
    }
}
