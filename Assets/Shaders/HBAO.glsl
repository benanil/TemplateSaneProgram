/*
* Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*     http://www.apache.org/licenses/LICENSE-2.0
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
* SPDX-FileCopyrightText: Copyright (c) 2014-2021 NVIDIA CORPORATION
* SPDX-License-Identifier: Apache-2.0
*/
/* 
Based on DeinterleavedTexturing sample by Louis Bavoil
https://github.com/NVIDIAGameWorks/D3DSamples/tree/master/samples/DeinterleavedTexturing
*/

// The pragma below is critical for optimal performance
// in this fragment shader to let the shader compiler
// fully optimize the maths and batch the texture fetches
// optimally
#pragma optionNV(unroll all)

#define M_PI 3.14159265

#define AO_RANDOMTEX_SIZE 4

layout(location = 0) out float result;

in vec2 texCoord;

uniform highp sampler2DArray uTexLinearDepth;
uniform sampler2D uTexNormal;
uniform vec4 uJitter;
uniform mat4 uView;
uniform mat4 uData;

// tweakables
const float NUM_STEPS = 4.0;
const float NUM_DIRECTIONS = 8.0; // texRandom/uJitter initialization depends on this

// bottom values from uData
float RadiusToScreen       ; // radius
float R2                   ; // 1.0 / radius
float NegInvR2             ; // radius * radius
float NDotVBias            ;
vec2  InvFullResolution    ;
vec2  InvQuarterResolution ;
vec4  projInfo             ;
float AOMultiplier         ;
float PowExponent          ;

//----------------------------------------------------------------------------------

vec3 UVToView(vec2 uv, float eye_z)
{
    return vec3((uv * projInfo.xy + projInfo.zw) * eye_z, eye_z);
}

vec3 FetchQuarterResViewPos(vec2 UV)
{
    float depthLayerIndex = uJitter.w;
    float ViewDepth = texture(uTexLinearDepth, vec3(UV, depthLayerIndex)).x;
    return UVToView(UV, ViewDepth);
}

//----------------------------------------------------------------------------------
float Falloff(float DistanceSquare)
{
    return DistanceSquare * NegInvR2 + 1.0;
}

//----------------------------------------------------------------------------------
// P = view-space position at the kernel center
// N = view-space normal at the kernel center
// S = view-space position of the current sample
//----------------------------------------------------------------------------------
float ComputeAO(vec3 P, vec3 N, vec3 S)
{
    vec3 V = S - P;
    float VdotV = dot(V, V);
    float NdotV = dot(N, V) * inversesqrt(VdotV);
    // Use saturate(x) instead of max(x,0.f) because that is faster on Kepler
    return clamp(NdotV - NDotVBias, 0.0, 1.0) * clamp(Falloff(VdotV), 0.0, 1.0);
}

//----------------------------------------------------------------------------------
vec2 RotateDirection(vec2 Dir, vec2 CosSin)
{
    return vec2(Dir.x * CosSin.x - Dir.y * CosSin.y,
                Dir.x * CosSin.y + Dir.y * CosSin.x);
}

//----------------------------------------------------------------------------------
float ComputeCoarseAO(vec2 FullResUV, float RadiusPixels, vec3 ViewPosition, vec3 ViewNormal)
{
    vec4 Rand = uJitter;
    RadiusPixels /= 4.0;

    // Divide by NUM_STEPS+1 so that the farthest samples are not fully attenuated
    float StepSizePixels = RadiusPixels / (NUM_STEPS + 1.0);

    const float Alpha = 2.0 * M_PI / NUM_DIRECTIONS;
    float AO = 0.0;

    for (float DirectionIndex = 0.0; DirectionIndex < NUM_DIRECTIONS; DirectionIndex += 1.0)
    {
        float Angle = Alpha * DirectionIndex;

        // Compute normalized 2D direction
        vec2 Direction = RotateDirection(vec2(cos(Angle), sin(Angle)), Rand.xy);

        // Jitter starting sample within the first step
        float RayPixels = (Rand.z * StepSizePixels + 1.0);

        for (float StepIndex = 0.0; StepIndex < NUM_STEPS; StepIndex += 1.0)
        {
            vec2 SnappedUV = round(RayPixels * Direction) * InvQuarterResolution + FullResUV;
            vec3 S = FetchQuarterResViewPos(SnappedUV);
            RayPixels += StepSizePixels;
            AO += ComputeAO(ViewPosition, ViewNormal, S);
        }
    }

    AO *= AOMultiplier / (NUM_DIRECTIONS * NUM_STEPS);
    return clamp(1.0 - AO * 2.0, 0.0, 1.0);
}

//----------------------------------------------------------------------------------
void main()
{
    RadiusToScreen       = uData[0][0];
    R2                   = uData[0][1];
    NegInvR2             = uData[0][2];
    NDotVBias            = uData[0][3];
    InvFullResolution    = uData[1].xy;
    InvQuarterResolution = uData[1].zw;
    projInfo             = uData[2]   ;
    AOMultiplier         = uData[3].x ;
    PowExponent          = uData[3].y ;

    vec2 offset = vec2(float(int(uJitter.w) & 3) + 0.5, uJitter.w * 0.25 + 0.5);
    
    vec2 base = floor(gl_FragCoord.xy) * 4.0 + offset;
    vec2 uv = base * (InvQuarterResolution / 4.0);
    vec3 ViewPosition = FetchQuarterResViewPos(uv);

    vec3 ViewNormal = texelFetch(uTexNormal, ivec2(base), 0).xyz * 2.0 - 1.0;
    ViewNormal = normalize(mat3(uView) * -ViewNormal);

    // Compute projection of disk of radius control.R into screen space
    float RadiusPixels = RadiusToScreen / ViewPosition.z;
    float AO = ComputeCoarseAO(uv, RadiusPixels, ViewPosition, ViewNormal);
    result = pow(AO, PowExponent);
}