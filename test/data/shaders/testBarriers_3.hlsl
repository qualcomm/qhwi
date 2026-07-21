//
//
//  testBarriers.hlsl
//  QHWI
//
//  Copyright (C) 2023 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct DrawReadTask0_VS_Input { float2 pos : POSITION0; };
struct DrawReadTask0_VS_Output { float4 pos: SV_POSITION; };

DrawReadTask0_VS_Output DrawReadTask0_VS_Main(DrawReadTask0_VS_Input input)
{
    DrawReadTask0_VS_Output output;
    output.pos = float4(input.pos, 0.5f, 1.0f);
    return output;
}

float4 DrawReadTask0_PS_Main(DrawReadTask0_VS_Output input) : SV_TARGET
{
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}

