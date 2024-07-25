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
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

//-----------------------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------------------
#ifndef MAX_EDGE_COUNT_BITS
#define MAX_EDGE_COUNT_BITS			4			// Default edge count bits is 4
#endif

#ifndef SHOW_EDGES
#define SHOW_EDGES					0			// Disabled by default      
#endif

#ifndef USE_STENCIL
#define USE_STENCIL					0			// Disabled by default      
#endif

//#define USE_GATHER                            // Disabled by default

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

layout(location = 0) out mediump uint uResult; // R8G8_int

uniform sampler2D uInputTex; // rgb is color a is luminance

uint EncodeCount(uint negCount, uint posCount)
{
	return ((negCount & kCountShiftMask) << kNegCountShift) | (posCount & kCountShiftMask);
}

//----------------------------------------------------------------------------
//	MLAA pixel shader for edge detection.
//	Pixel shader used in the first phase of MLAA.
//	This pixel shader is used to detect vertical and horizontal edges.
//-----------------------------------------------------------------------------
uint MLAA_SeperatingLines(sampler2D Sampler, ivec2 Offset)
{
    vec2 center;
    vec2 upright;
    
    center.xy = texelFetch(Sampler, Offset            , 0).aa; // texelFetch(Sampler, clamp(Offset, ivec2(0, 0), TextureSize), 0).aa;
    upright.y = texelFetch(Sampler, Offset + kUp.xy   , 0).a;  // texelFetch(Sampler, clamp(Offset + kUp.xy, ivec2(0, 0), TextureSize), 0).a;
    upright.x = texelFetch(Sampler, Offset + kRight.xy, 0).a;  // texelFetch(Sampler, clamp(Offset + kRight.xy, ivec2(0, 0), TextureSize), 0).a;
    
    uint rVal = 0u;
    
    bvec2 result = greaterThan(abs(center - upright), vec2(fInvEdgeDetectionTreshold)); // CompareColors2(center, upright);
    
    // Check for seperating lines
    if (result.y)
        rVal |= kUpperMask;
    if (result.x)
        rVal |= kRightMask;
    
    return rVal;
}

uvec4 select(uvec4 a, uvec4 b, uvec4 test)
{
    return (a & ~test) | (b & test);
}

//-----------------------------------------------------------------------------
//	Pixel shader for the second phase of the algorithm.
//	This pixel shader calculates the length of edges.
//-----------------------------------------------------------------------------
// MLAA_ComputeLineLength
void main()
{
    ivec2 Offset = ivec2(gl_FragCoord.xy);
    // Retrieve edge mask for current pixel	
    uint pixel = MLAA_SeperatingLines(uInputTex, Offset);
    uvec4 EdgeCount = uvec4(0, 0, 0, 0); // x = Horizontal Count Negative, y = Horizontal Count Positive, z = Vertical Count Negative, w = Vertical Count Positive				    
    
    // We use a single branch for vertical and horizontal edge testing
    // Doing this is faster than two different branches (one for vertical, one for horizontal)
    // In most case both V and H edges are spatially coherent (apart from purely horizontal or 
    // vertical edges but those don't happen often compared to other cases).				
    
    if ((pixel & (kUpperMask | kRightMask)) != 0u)
    {
        uvec4 EdgeDirMask = uvec4(kUpperMask, kUpperMask, kRightMask, kRightMask);
        uvec4 EdgeFound = uvec4(equal(pixel & EdgeDirMask, uvec4(0u))) - uvec4(1u);
        // Nullify the stopbit if we're not supposed to look at this edge
        uvec4 StopBit = uvec4(kStopBit) & EdgeFound; 

        for (int i = 1; i <= int(kMaxEdgeLength); i++)
        {
            uvec4 uEdgeMask;
            uEdgeMask.x = MLAA_SeperatingLines(uInputTex, Offset + ivec2(-i,  0));
            uEdgeMask.y = MLAA_SeperatingLines(uInputTex, Offset + ivec2( i,  0));
            uEdgeMask.z = MLAA_SeperatingLines(uInputTex, Offset + ivec2( 0,  i));
            uEdgeMask.w = MLAA_SeperatingLines(uInputTex, Offset + ivec2( 0, -i));

            EdgeFound = EdgeFound & (uEdgeMask & EdgeDirMask);
            uvec4 mask = uvec4(equal(EdgeFound, uvec4(0u))) - uvec4(1u);
            EdgeCount = select(EdgeCount | StopBit, EdgeCount + uvec4(1u), mask);
        }
    }
    
    uResult  = EncodeCount(EdgeCount.x, EdgeCount.y);
    uResult |= EncodeCount(EdgeCount.z, EdgeCount.w) << 8u;
}
