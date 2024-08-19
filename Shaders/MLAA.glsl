// AMD Morphological Anti-Aliasing (MLAA) Sample
//
// https://github.com/GPUOpen-LibrariesAndSDKs/MLAA11
//
// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

//-----------------------------------------------------------------------------------------
// apply Morphological Anti-Aliasing (MLAA) to a scene
// as a post-process operation.
// GLSL-Port 2023 by Denis Reischl https://www.shadertoy.com/view/cllXRB
//-----------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------------------
#ifndef MAX_EDGE_COUNT_BITS
#define MAX_EDGE_COUNT_BITS  4   // Default edge count bits is 4
#endif                           
                                 
#ifndef SHOW_EDGES               
#define SHOW_EDGES           0   // Disabled by default      
#endif                           
                                 
#ifndef USE_STENCIL              
#define USE_STENCIL          0   // Disabled by default      
#endif

//-----------------------------------------------------------------------------------------
// Static Constants
//-----------------------------------------------------------------------------------------
// Set the number of bits to use when storing the horizontal and vertical counts
// This number should be half the number of bits in the color channels used
// E.g. with a RT format of DXGI_R8G8_int this number should be 8/2 = 4
// Longer edges can be detected by increasing this number; however this requires a 
// larger bit depth format, and also makes the edge length detection function slower
const uint kNumCountBits = uint(MAX_EDGE_COUNT_BITS);

// The maximum edge length that can be detected
const uint kMaxEdgeLength = ((1u << (kNumCountBits - 1u)) - 1u);

// Various constants used by the shaders below
const uint kUpperMask             = (1u << 0u);
const uint kUpperMask_BitPosition = 0u;
const uint kRightMask             = (1u << 1u);
const uint kRightMask_BitPosition = 1u;
const uint kStopBit               = (1u << (kNumCountBits - 1u));
const uint kStopBit_BitPosition   = (kNumCountBits - 1u);
const uint kNegCountShift         = (kNumCountBits);
const uint kPosCountShift         = (00u);
const uint kCountShiftMask        = ((1u << kNumCountBits) - 1u);

const ivec3 kZero  = ivec3( 0,  0, 0);
const ivec3 kUp    = ivec3( 0, -1, 0);
const ivec3 kDown  = ivec3( 0,  1, 0);
const ivec3 kRight = ivec3( 1,  0, 0);
const ivec3 kLeft  = ivec3(-1,  0, 0);

// This constant defines the luminance intensity difference to check for when testing any 
// two pixels for an edge.
const float fInvEdgeDetectionTreshold = 1.f / 32.f;

layout(location = 0) out lowp vec4 fResult;

in vec2 texCoord;

uniform lowp sampler2D uColorTex;
uniform mediump usampler2D uEdgesTex;
uniform lowp sampler2D uGodRaysTex;
uniform lowp sampler2D uAmbientOcclussion;

//-----------------------------------------------------------------------------------------
// Utility functions
//-----------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
// Returns true if the colors are different
//--------------------------------------------------------------------------------------
bool CompareColors(lowp float a, lowp float b)
{
	return (abs(a - b) > fInvEdgeDetectionTreshold);
}

//--------------------------------------------------------------------------------------
// Check if the specified bit is set
//--------------------------------------------------------------------------------------
bool IsBitSet(mediump uint Value, const mediump uint uBitPosition)
{
    return (Value & (1u << uBitPosition)) > 0u;
}
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
mediump uint RemoveStopBit(mediump uint a)
{
    return a & (kStopBit - 1u);
}
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
mediump uint DecodeCountNoStopBit(mediump uint count, mediump uint shift)
{
    return RemoveStopBit((count >> shift) & kCountShiftMask);
}
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
mediump uint DecodeCount(mediump uint count, mediump uint shift)
{
    return (count >> shift) & kCountShiftMask;
}
//--------------------------------------------------------------------------------------

lowp vec4 toLinear4(lowp vec4 sRGB)
{
    // https://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
    return sRGB * (sRGB * (sRGB * 0.305306011 + 0.682171111) + 0.012522878);
}

//-----------------------------------------------------------------------------	
//  Main function used in third and final phase of the algorithm
//  This code reads previous inputs and perform anti-aliasing of edges by 
//  blending colors as required.
//-----------------------------------------------------------------------------
void BlendColor(mediump uint count,
                ivec2 pos,
                ivec2 dir,
                ivec2 ortho,
                bool _inverse,
                inout lowp vec4 color)
{
    // Only process pixel edge if it contains a stop bit
    if (IsBitSet(count, kStopBit_BitPosition + kPosCountShift) || IsBitSet(count, kStopBit_BitPosition + kNegCountShift))
    {
        // Retrieve edge length
        mediump uint negCount = DecodeCountNoStopBit(count, kNegCountShift);
        mediump uint posCount = DecodeCountNoStopBit(count, kPosCountShift);

        // Fetch color adjacent to the edge
        lowp vec4 adjacentcolor = toLinear4(texelFetch(uColorTex, pos + dir, 0));

        if ((negCount + posCount) == 0u)
        {
            const lowp float weight = 1.0 / 8.0; // Arbitrary			
            // Cheap approximation of gamma to linear and then back again
            color = mix(color, adjacentcolor, weight);
            return;
        }
        else
        {
            // If no sign bit is found on either edge then artificially increase the edge length so that
            // we don't start anti-aliasing pixels for which we don't have valid data.
            if (!(IsBitSet(count, (kStopBit_BitPosition + kPosCountShift)))) posCount = kMaxEdgeLength + 1u;
            if (!(IsBitSet(count, (kStopBit_BitPosition + kNegCountShift)))) negCount = kMaxEdgeLength + 1u;

            // Calculate some variables
            float _length   = float(negCount + posCount) + 1.f;
            float midPoint  = _length / 2.f;
            float _distance = float(negCount);

            const mediump uint upperU   = 0x00u;
            const mediump uint risingZ  = 0x01u;
            const mediump uint fallingZ = 0x02u;
            const mediump uint lowerU   = 0x03u;

            ///////////////////////////////////////////////////////////////////////////////////////
            // Determining what pixels to blend
            // 4 possible values for shape - x indicates a blended pixel:
            //
            // 0: |xxxxxx| -> (h0 > 0) && (h1 > 0) : upperU     - blend along the entire inverse edge
            //     ------
            //
            //
            // 1:     xxx| -> (h0 < 0) && (h1 > 0) : risingZ    - blend first half on inverse, 
            //     ------                                         blend second half on non-inverse
            //    |xxx                                            
            //
            // 2: |xxx     -> (h0 > 0) && (h1 < 0) : fallingZ   - blend first half on non-inverse, 
            //     ------                                         blend second half on inverse
            //        xxx|                                        
            //
            // 3:          -> (h0 < 0) && (h1 < 0) : lowerU     - blend along the entire non-inverse edge
            //     ------
            //    |xxxxxx|
            ///////////////////////////////////////////////////////////////////////////////////////

            mediump uint shape = 0x00u;
            if (CompareColors((texelFetch(uColorTex, pos - (ortho * ivec2(int(negCount))), 0).a), 
                              (texelFetch(uColorTex, pos - (ortho * (ivec2(int(negCount) + 1))), 0).a)))
            {
                shape |= risingZ;
            }

            if (CompareColors((texelFetch(uColorTex, pos + (ortho * ivec2(int(posCount))), 0).a), 
                              (texelFetch(uColorTex, pos + (ortho * (ivec2(int(posCount) + 1))), 0).a)))
            {
                shape |= fallingZ;
            }

            // Parameter "_inverse" is hard-coded on call so will not generate a dynamic branch condition
            if ((_inverse && (((shape == fallingZ) && (float(negCount) <= midPoint)) ||
                ((shape == risingZ) && (float(negCount) >= midPoint)) ||
                ((shape == upperU))))
                || (!_inverse && (((shape == fallingZ) && (float(negCount) >= midPoint)) ||
                    ((shape == risingZ) && (float(negCount) <= midPoint)) ||
                    ((shape == lowerU)))))
            {
                float h0 = abs((1.0 / _length) * (_length - _distance) - 0.5);
                float h1 = abs((1.0 / _length) * (_length - _distance - 1.0) - 0.5);
                float area = 0.5f * (h0 + h1);                
                // Cheap approximation of gamma to linear and then back again
                color = mix(color, adjacentcolor, area);
            }
        }
    }
}

//-----------------------------------------------------------------------------
//	MLAA pixel shader for color blending.
//	Pixel shader used in third and final phase of the algorithm
//-----------------------------------------------------------------------------
lowp vec4 MLAA_BlendColor_PS(ivec2 Offset, bool bShowEdgesOnly)
{
    lowp vec4 rVal   = toLinear4(texelFetch(uColorTex, Offset, 0));
    mediump uint edges  = texelFetch(uEdgesTex, Offset, 0).x;
    mediump uint hcount = edges & 0xFFu;
    mediump uint vcount = (edges >> 8u) & 0xFFu;
    
    // if (bShowEdgesOnly) // test visualize edges
    // {
    //     if ((hcount != 0u) || (vcount != 0u))
    //     {
    //         if ((IsBitSet(hcount, kStopBit_BitPosition + kPosCountShift) || IsBitSet(hcount, kStopBit_BitPosition + kNegCountShift)) ||
    //             (IsBitSet(vcount, kStopBit_BitPosition + kPosCountShift) || IsBitSet(vcount, kStopBit_BitPosition + kNegCountShift)))
    //         {
    //             uint Count = 0u;
    //             Count += DecodeCountNoStopBit(hcount, kNegCountShift);
    //             Count += DecodeCountNoStopBit(hcount, kPosCountShift);
    //             Count += DecodeCountNoStopBit(vcount, kNegCountShift);
    //             Count += DecodeCountNoStopBit(vcount, kPosCountShift);
    //             if (Count != 0u)
    //             rVal = vec4(1, 0, 0, 1);
    //         }
    //     }
    // }
    // else
    // {
    // }
    mediump uint hcountup    =  texelFetch(uEdgesTex, Offset - kUp.xy, 0).x & 0xFFu;
    mediump uint vcountright = (texelFetch(uEdgesTex, Offset - kRight.xy, 0).x >> 8u) & 0xFFu;
    
    // Blend pixel colors as required for anti-aliasing edges
    if (hcount     != 0u) BlendColor(hcount, Offset, kUp.xy, kRight.xy, false, rVal);   // H down-up
    if (hcountup   != 0u) BlendColor(hcountup, Offset - kUp.xy, -kUp.xy, kRight.xy, true, rVal);   // H up-down    				    
    if (vcount     != 0u) BlendColor(vcount, Offset, kRight.xy, kUp.xy, false, rVal);   // V left-right				
    if (vcountright!= 0u) BlendColor(vcountright, Offset - kRight.xy, -kRight.xy, kUp.xy, true, rVal);   // V right-left    			

    return rVal;
}

//------------------------------------------------------------------------
// post processing
vec3 ACESFilm(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// https://www.shadertoy.com/view/WdjSW3
// equivalent to reinhard tone mapping but no need to gamma correction
vec3 CustomToneMapping(vec3 x)
{
    return pow(ACESFilm(x), vec3(1.0 / 2.2));
}

// https://www.shadertoy.com/view/lsKSWR
float Vignette(vec2 uv)
{
    uv *= vec2(1.0) - uv.yx;   // vec2(1.0)- uv.yx; -> 1.-u.yx; Thanks FabriceNeyret !
    return pow(uv.x * uv.y * 15.0, 0.15); // change pow for modifying the extend of the  vignette
}

// gaussian blur
lowp float Blur5(lowp float a, lowp float b, lowp float c, lowp float d, lowp float e) 
{
    const lowp float Weights5[3] = float[3](6.0f / 16.0f, 4.0f / 16.0f, 1.0f / 16.0f);
    return Weights5[0] * a + Weights5[1] * (b + c) + Weights5[2] * (d + e);
}

void main()
{
    ivec2 iTexCoord = ivec2(gl_FragCoord.xy);
    vec3 godRays = vec3(0.8, 0.65, 0.58) * texelFetch(uGodRaysTex, iTexCoord, 0).r;
    
    // vertical blur
    lowp float ao = Blur5(texelFetch(uAmbientOcclussion, iTexCoord + ivec2(0,  0), 0).r,
                          texelFetch(uAmbientOcclussion, iTexCoord + ivec2(0, -1), 0).r,
                          texelFetch(uAmbientOcclussion, iTexCoord + ivec2(0,  1), 0).r,
                          texelFetch(uAmbientOcclussion, iTexCoord + ivec2(0, -2), 0).r,
                          texelFetch(uAmbientOcclussion, iTexCoord + ivec2(0,  2), 0).r);
    // // ao *= min(albedoShadow.w * 3.0, 1.0);
    // ao += 0.05;
    // ao = min(1.0, ao * 1.125);

    vec3 mlaa = MLAA_BlendColor_PS(iTexCoord, false).rgb;
    fResult.rgb = CustomToneMapping(mlaa) * Vignette(texCoord) * ao;
    fResult.rgb += godRays;
    fResult.a = 1.0;
}