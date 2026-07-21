//
//
//  qhwi_d3d_internal_extensions.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#pragma once

#include <stdio.h>
#include <qhwi/qhwi_enums.h>
#include <qre/text/textutil.h>
#include <qre/common.h>
#include <d3d12.h>

namespace qhwi_d3d_internal_extensions
{
static const int MaxRootParameters = 32;
static const int MaxStringSize = 128;

/*Filename related strings*/
static const char* GenericFilePath = "C:\\Users\\Public\\";
static const char* GenericFilenamePrefix = "dxApi";

/*Type Names*/
static const char d3d12DescriptorRangeTypeName[] = "D3D12_DESCRIPTOR_RANGE1";
static const char d3d12DescriptorTableTypeName[] = "D3D12_ROOT_DESCRIPTOR_TABLE1";
static const char d3d12DescriptorParamTypeName[] = "D3D12_ROOT_PARAMETER1";
static const char d3d12RootDescriptorTypeName[] = "D3D12_ROOT_DESCRIPTOR1";
static const char d3d12RootConstantTypeName[] = "D3D12_ROOT_CONSTANTS";
static const char d3d12RootSignature1_1TypeName[] = "D3D12_ROOT_SIGNATURE_DESC1";
static const char d3d12VersionedRSDescTypeName[] = "D3D12_VERSIONED_ROOT_SIGNATURE_DESC";

/*Variable Names*/
static const char descriptorRangeVariableName[] = "descRangeType";
static const char descriptorTableVariableName[] = "descTableType";
static const char rootParamVariableName[] = "rootParamType";
static const char rootDescriptorVariableName[] = "rootDescriptor";
static const char rootConstantVariableName[] = "rootConstant";
static const char rootCbvDescriptorVariableName[] = "rootCbvDesc";
static const char rootSrvDescriptorVariableName[] = "rootSrvDesc";
static const char rootUavDescriptorVariableName[] = "rootUavDesc";
static const char srCbUavDescTableVariableName[] = "pSrCbUavDescTable";
static const char constantsTableVariableName[] = "pConstantsTable";
static const char rootParamTableVariableName[] = "pRootParameterTable";
static const char rootSignature1_1VariableName[] = "rootSignature1_1";
static const char d3d12VersionedRSDescVariableName[] = "versionedRootSignatureDesc";

static const char* enumD3D12_ROOT_DESCRIPTOR_FLAGS[] = {
    "D3D12_ROOT_DESCRIPTOR_FLAG_NONE",
    "D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE",
    "D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE",
    "D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC",
};

static const char* enumD3D12_ROOT_PARAMETER_TYPE[] = {

    "D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE",
    "D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS",
    "D3D12_ROOT_PARAMETER_TYPE_CBV",
    "D3D12_ROOT_PARAMETER_TYPE_SRV",
    "D3D12_ROOT_PARAMETER_TYPE_UAV",
};

static const char* enumD3D12_SHADER_VISIBILITY[] = {
    "D3D12_SHADER_VISIBILITY_ALL",      "D3D12_SHADER_VISIBILITY_VERTEX", "D3D12_SHADER_VISIBILITY_HULL",          "D3D12_SHADER_VISIBILITY_DOMAIN",
    "D3D12_SHADER_VISIBILITY_GEOMETRY", "D3D12_SHADER_VISIBILITY_PIXEL",  "D3D12_SHADER_VISIBILITY_AMPLIFICATION", "D3D12_SHADER_VISIBILITY_MESH",
};

static const char* enumD3D12_DESCRIPTOR_RANGE_TYPE[] = {
    "D3D12_DESCRIPTOR_RANGE_TYPE_SRV",
    "D3D12_DESCRIPTOR_RANGE_TYPE_UAV",
    "D3D12_DESCRIPTOR_RANGE_TYPE_CBV",
    "D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER",
};

static const char* enumD3D12_DESCRIPTOR_RANGE_FLAGS[] = {
    "D3D12_DESCRIPTOR_RANGE_FLAG_NONE",          "D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE",
    "D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE", "D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE",
    "D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC",   "D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS",
};

static const char* enumD3D12_ROOT_SIGNATURE_FLAGS[] = {
    "D3D12_ROOT_SIGNATURE_FLAG_NONE",
    "D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT",
    "D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS",
    "D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS",
    "D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS",
    "D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS",
    "D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS",
    "D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT",
    "D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE",
    "D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS",
    "D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS",
    "D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED",
    "D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED",
};

static const char* enumD3D_ROOT_SIGNATURE_VERSION[] = {
    "D3D_ROOT_SIGNATURE_VERSION_1",
    "D3D_ROOT_SIGNATURE_VERSION_1_0",
    "D3D_ROOT_SIGNATURE_VERSION_1_1",
    "D3D_ROOT_SIGNATURE_VERSION_1_2",
};

static const char* rangeTypeVariableNameTable[] = { "Srv", "Uav", "Cbv", "Sampler" };

class ExportHeader
{

public:
    ExportHeader(const char* shadername, bool exportOption)
    {
        m_exportOption = exportOption;

        QRE_ASSERT(shadername != nullptr);
        if (m_exportOption == true)
        {
            FileOpen(shadername);
        }
    };

    ~ExportHeader()
    {
        if (m_exportOption == true)
        {
            FileClose();
        }

        m_exportOption = false;
    }

    void WriteFileHeader();
    void WriteFileFooter();
    void ExportDxil(const void* shader, unsigned int len);
    void ExportDescriptorRange1(unsigned int bindingIndex, D3D12_DESCRIPTOR_RANGE1 r);
    void ExportRootParamAsDescriptorTable(unsigned int bindingIndex, D3D12_ROOT_PARAMETER1 p);
    void ExportRootParamAsConstants(unsigned int bindingIndex, D3D12_ROOT_PARAMETER1 p);
    void ExportRootParamAsConstantBufferView(unsigned int bindingIndex, D3D12_ROOT_PARAMETER1 p);
    void ExportRootParamAsShaderResourceView(unsigned int bindingIndex, D3D12_ROOT_PARAMETER1 p);
    void ExportRootParamAsUnorderedAccessView(unsigned int bindingIndex, D3D12_ROOT_PARAMETER1 p);
    void ExportRootSignature_1_1(D3D12_VERSIONED_ROOT_SIGNATURE_DESC rs);

private:
    int GenericWrite(const char* pFmt, ...);
    void FileOpen(const char* shaderName);
    void FileClose();
    int m_filenameLen;
    int m_fullFilenameLen;
    int m_guardnameLen;
    int m_shadernameLen;
    char* m_filename;
    char* m_fullFilename;
    char* m_guardname;
    char* m_shadername;
    FILE* m_fileHandle;
    bool m_exportOption;
};
} // namespace qhwi_d3d_internal_extensions