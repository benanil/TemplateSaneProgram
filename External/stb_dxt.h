// stb_dxt.h - Real-Time DXT1/DXT5 compressor 
// Based on original by fabian "ryg" giesen v1.04
// Custom version, modified by Yann Collet
//
/*
   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - RygsDXTc source repository : http://code.google.com/p/rygsdxtc/

*/
// use '#define STB_DXT_IMPLEMENTATION' before including to create the implementation
//
// USAGE:
//   call stb_compress_dxt_block() for every block (you must pad)
//     source should be a 4x4 block of RGBA data in row-major order;
//     A is ignored if you specify alpha=0; you can turn on dithering
//     and "high quality" using mode.
//
// version history:
//   v1.06  - (cyan) implement Fabian Giesen's comments
//   v1.05  - (cyan) speed optimizations
//   v1.04  - (ryg) default to no rounding bias for lerped colors (as per S3TC/DX10 spec);
//            single color match fix (allow for inexact color interpolation);
//            optimal DXT5 index finder; "high quality" mode that runs multiple refinement steps.
//   v1.03  - (stb) endianness support
//   v1.02  - (stb) fix alpha encoding bug
//   v1.01  - (stb) fix bug converting to RGB that messed up quality, thanks ryg & cbloom
//   v1.00  - (stb) first release

#ifndef STB_INCLUDE_STB_DXT_H
#define STB_INCLUDE_STB_DXT_H

#include <immintrin.h>

//*******************************************************************
// Enable custom Optimisations
// Comment this define if you want to revert to ryg's original code
#define NEW_OPTIMISATIONS
//*******************************************************************

// compression mode (bitflags)
#define STB_DXT_HIGHQUAL  2   // high quality mode, does two refinement steps instead of 1. ~30-40% slower.

void rygCompress( unsigned char *dst, const unsigned char *src, int w, int h);

#define STB_COMPRESS_DXT_BLOCK

#ifdef STB_DXT_IMPLEMENTATION

// configuration options for DXT encoder. set them in the project/makefile or just define
// them at the top.

// STB_DXT_USE_ROUNDING_BIAS
//     use a rounding bias during color interpolation. this is closer to what "ideal"
//     interpolation would do but doesn't match the S3TC/DX10 spec. old versions (pre-1.03)
//     implicitly had this turned on. 
//
//     in case you're targeting a specific type of hardware (e.g. console programmers):
//     NVidia and Intel GPUs (as of 2010) as well as DX9 ref use DXT decoders that are closer
//     to STB_DXT_USE_ROUNDING_BIAS. AMD/ATI, S3 and DX10 ref are closer to rounding with no bias.
//     you also see "(a*5 + b*3) / 8" on some old GPU designs.
// #define STB_DXT_USE_ROUNDING_BIAS

#include <stdlib.h>
#include <math.h>
#include <stddef.h>
#include <string.h> // memset
#include <assert.h>
#include <iostream>
#include <algorithm>

static unsigned char stb__Expand5[32];
static unsigned char stb__Expand6[64];
static unsigned char stb__OMatch5[256][2];
static unsigned char stb__OMatch6[256][2];
static unsigned char stb__QuantRBTab[256+16];
static unsigned char stb__QuantGTab[256+16];

static int stb__Mul8Bit(int a, int b)
{
  int t = a*b + 128;
  return (t + (t >> 8)) >> 8;
}

static void stb__From16Bit(unsigned char *out, unsigned short v)
{
   int rv = (v & 0xf800) >> 11;
   int gv = (v & 0x07e0) >>  5;
   int bv = (v & 0x001f) >>  0;

   out[0] = stb__Expand5[rv];
   out[1] = stb__Expand6[gv];
   out[2] = stb__Expand5[bv];
   out[3] = 0;
}

static unsigned short stb__As16Bit(int r, int g, int b)
{
   return (stb__Mul8Bit(r,31) << 11) + (stb__Mul8Bit(g,63) << 5) + stb__Mul8Bit(b,31);
}

// linear interpolation at 1/3 point between a and b, using desired rounding type
static int stb__Lerp13(int a, int b)
{
#ifdef STB_DXT_USE_ROUNDING_BIAS
   // with rounding bias
   return a + stb__Mul8Bit(b-a, 0x55);
#else
   // without rounding bias
   // replace "/ 3" by "* 0xaaab) >> 17" if your compiler sucks or you really need every ounce of speed.
   return (2*a + b) / 3;
#endif
}

// lerp RGB color
static void stb__Lerp13RGB(unsigned char *out, unsigned char *p1, unsigned char *p2)
{
   out[0] = stb__Lerp13(p1[0], p2[0]);
   out[1] = stb__Lerp13(p1[1], p2[1]);
   out[2] = stb__Lerp13(p1[2], p2[2]);
}

/****************************************************************************/

// compute table to reproduce constant colors as accurately as possible
static void stb__PrepareOptTable(unsigned char *Table,const unsigned char *expand,int size)
{
   int i,mn,mx;
   for (i=0;i<256;i++) {
      int bestErr = 256;
      for (mn=0;mn<size;mn++) {
         for (mx=0;mx<size;mx++) {
            int mine = expand[mn];
            int maxe = expand[mx];
            int err = abs(stb__Lerp13(maxe, mine) - i);
            
            // DX10 spec says that interpolation must be within 3% of "correct" result,
            // add this as error term. (normally we'd expect a random distribution of
            // +-1.5% error, but nowhere in the spec does it say that the error has to be
            // unbiased - better safe than sorry).
            err += abs(maxe - mine) * 3 / 100;
            
            if(err < bestErr)
            { 
               Table[i*2+0] = mx;
               Table[i*2+1] = mn;
               bestErr = err;
            }
         }
      }
   }
}

static void stb__EvalColors(unsigned char *color,unsigned short c0,unsigned short c1)
{
   stb__From16Bit(color+ 0, c0);
   stb__From16Bit(color+ 4, c1);
   stb__Lerp13RGB(color+ 8, color+0, color+4);
   stb__Lerp13RGB(color+12, color+4, color+0);
}

// The color matching function
static unsigned int stb__MatchColorsBlock(unsigned char *block, unsigned char *color)
{
   unsigned int mask = 0;
   int dirr = color[0*4+0] - color[1*4+0];
   int dirg = color[0*4+1] - color[1*4+1];
   int dirb = color[0*4+2] - color[1*4+2];
   int dots[16];
   int stops[4];
   int i;
   int c0Point, halfPoint, c3Point;

   for(i=0;i<16;i++)
      dots[i] = block[i*4+0]*dirr + block[i*4+1]*dirg + block[i*4+2]*dirb;

   for(i=0;i<4;i++)
      stops[i] = color[i*4+0]*dirr + color[i*4+1]*dirg + color[i*4+2]*dirb;

   // think of the colors as arranged on a line; project point onto that line, then choose
   // next color out of available ones. we compute the crossover points for "best color in top
   // half"/"best in bottom half" and then the same inside that subinterval.
   //
   // relying on this 1d approximation isn't always optimal in terms of euclidean distance,
   // but it's very close and a lot faster.
   // http://cbloomrants.blogspot.com/2008/12/12-08-08-dxtc-summary.html
   
   c0Point   = (stops[1] + stops[3]) >> 1;
   halfPoint = (stops[3] + stops[2]) >> 1;
   c3Point   = (stops[2] + stops[0]) >> 1;

      // the version without dithering is straightforward

   const int indexMap[8] = { 0 << 30,2 << 30,0 << 30,2 << 30,3 << 30,3 << 30,1 << 30,1 << 30 };
   
   for(int i=0;i<16;i++)
   {
     int dot = dots[i];
     mask >>= 2;
   
     int bits =( (dot < halfPoint) ? 4 : 0 )
             | ( (dot < c0Point) ? 2 : 0 )
             | ( (dot < c3Point) ? 1 : 0 );
   
     mask |= indexMap[bits];
   }

   return mask;
}

// The color optimization function. (Clever code, part 1)
static void stb__OptimizeColorsBlock(unsigned char *block, unsigned short *pmax16, unsigned short *pmin16)
{
  unsigned char *minp, *maxp;
  double magn;
  int v_r,v_g,v_b;
  static const int nIterPower = 4;
  float covf[6],vfr,vfg,vfb;

  // determine color distribution
  int cov[6];
  int mu[3],min[3],max[3];
  int ch,i,iter;

  for(ch=0;ch<3;ch++)
  {
    const unsigned char *bp = ((const unsigned char *) block) + ch;
    int muv,minv,maxv;

#ifdef NEW_OPTIMISATIONS
#   define DXMIN(a,b)      (int)a + ( ((int)b-a) & ( ((int)b-a) >> 31 ) )
#   define DXMAX(a,b)      (int)a + ( ((int)b-a) & ( ((int)a-b) >> 31 ) )
#   define RANGE(a,b,n)  int min##n = DXMIN(a,b); int max##n = a+b - min##n; muv += a+b;
#   define MINMAX(a,b,n) int min##n = DXMIN(min##a, min##b); int max##n = DXMAX(max##a, max##b); 

	muv = 0;
	RANGE(bp[0],  bp[4],  1);
	RANGE(bp[8],  bp[12], 2);
	RANGE(bp[16], bp[20], 3);
	RANGE(bp[24], bp[28], 4);
	RANGE(bp[32], bp[36], 5);
	RANGE(bp[40], bp[44], 6);
	RANGE(bp[48], bp[52], 7);
	RANGE(bp[56], bp[60], 8);

	MINMAX(1,2,9);
	MINMAX(3,4,10);
	MINMAX(5,6,11);
	MINMAX(7,8,12);

	MINMAX(9,10,13);
	MINMAX(11,12,14);

	minv = DXMIN(min13,min14);
	maxv = DXMAX(max13,max14);

#else
	muv = minv = maxv = bp[0];
    for(i = 4; i < 64; i += 4)
    {
      muv += bp[i];
      if (bp[i] < minv) minv = bp[i];
      else if (bp[i] > maxv) maxv = bp[i];
    }
#endif

    mu[ch] = (muv + 8) >> 4;
    min[ch] = minv;
    max[ch] = maxv;
  }

  // determine covariance matrix
  for (i=0;i<6;i++)
     cov[i] = 0;

  for (i = 0; i < 16; i++)
  {
    int r = block[i*4+0] - mu[0];
    int g = block[i*4+1] - mu[1];
    int b = block[i*4+2] - mu[2];

    cov[0] += r*r;
    cov[1] += r*g;
    cov[2] += r*b;
    cov[3] += g*g;
    cov[4] += g*b;
    cov[5] += b*b;
  }

  // convert covariance matrix to float, find principal axis via power iter
  for(i=0;i<6;i++)
    covf[i] = cov[i] / 255.0f;

  vfr = (float) (max[0] - min[0]);
  vfg = (float) (max[1] - min[1]);
  vfb = (float) (max[2] - min[2]);

  for(iter = 0; iter < nIterPower; iter++)
  {
    float r = vfr * covf[0] + vfg * covf[1] + vfb * covf[2];
    float g = vfr * covf[1] + vfg * covf[3] + vfb * covf[4];
    float b = vfr * covf[2] + vfg * covf[4] + vfb * covf[5];

    vfr = r;
    vfg = g;
    vfb = b;
  }

  magn = fabs(vfr);
  if (fabs(vfg) > magn) magn = fabs(vfg);
  if (fabs(vfb) > magn) magn = fabs(vfb);

  if(magn < 4.0f) 
  { // too small, default to luminance
     v_r = 299; // JPEG YCbCr luma coefs, scaled by 1000.
     v_g = 587;
     v_b = 114;
  } else {
     magn = 512.0 / magn;
     v_r = (int) (vfr * magn);
     v_g = (int) (vfg * magn);
     v_b = (int) (vfb * magn);
  }

  // Pick colors at extreme points
  int mind, maxd;
  mind = maxd = block[0]*v_r + block[1]*v_g + block[2]*v_b;
  minp = maxp = block;
  for(i=1;i<16;i++)
  {
     int dot = block[i*4+0]*v_r + block[i*4+1]*v_g + block[i*4+2]*v_b;
  
     if (dot < mind) {
        mind = dot;
        minp = block+i*4;
  	    continue;
     }
  
     if (dot > maxd) {
        maxd = dot;
        maxp = block+i*4;
     }
  }
  
  *pmax16 = stb__As16Bit(maxp[0],maxp[1],maxp[2]);
  *pmin16 = stb__As16Bit(minp[0],minp[1],minp[2]);
}

inline static int stb__sclamp(float y, int p0, int p1)
{
    int x = (int) y;
    x = x>p1 ? p1 : x;
    return x<p0 ? p0 : x;
}

// The refinement function. (Clever code, part 2)
// Tries to optimize colors to suit block contents better.
// (By solving a least squares system via normal equations+Cramer's rule)
static int stb__RefineBlock(unsigned char* block, unsigned short* pmax16, unsigned short* pmin16, unsigned int mask)
{
    static const int w1Tab[4] = { 3,0,2,1 };
    static const int prods[4] = { 0x090000,0x000900,0x040102,0x010402 };
    // ^some magic to save a lot of multiplies in the accumulating loop...
    // (precomputed products of weights for least squares system, accumulated inside one 32-bit register)

    float frb, fg;
    unsigned short oldMin, oldMax, min16, max16;
    int i, akku = 0, xx, xy, yy;
    int At1_r, At1_g, At1_b;
    int At2_r, At2_g, At2_b;
    unsigned int cm = mask;

    oldMin = *pmin16;
    oldMax = *pmax16;

    if ((mask ^ (mask << 2)) < 4) // all pixels have the same index?
    {
        // yes, linear system would be singular; solve using optimal
        // single-color match on average color
        int r = 8, g = 8, b = 8;
        for (i = 0; i < 16; ++i) {
            r += block[i * 4 + 0];
            g += block[i * 4 + 1];
            b += block[i * 4 + 2];
        }

        r >>= 4; g >>= 4; b >>= 4;

        max16 = (stb__OMatch5[r][0] << 11) | (stb__OMatch6[g][0] << 5) | stb__OMatch5[b][0];
        min16 = (stb__OMatch5[r][1] << 11) | (stb__OMatch6[g][1] << 5) | stb__OMatch5[b][1];
    }
    else {
        At1_r = At1_g = At1_b = 0;
        At2_r = At2_g = At2_b = 0;
        for (i = 0; i < 16; ++i, cm >>= 2)
        {
            int step = cm & 3;
            int w1 = w1Tab[step];
            int r = block[i * 4 + 0];
            int g = block[i * 4 + 1];
            int b = block[i * 4 + 2];

            akku += prods[step];
            At1_r += w1 * r;
            At1_g += w1 * g;
            At1_b += w1 * b;
            At2_r += r;
            At2_g += g;
            At2_b += b;
        }

        At2_r = 3 * At2_r - At1_r;
        At2_g = 3 * At2_g - At1_g;
        At2_b = 3 * At2_b - At1_b;

        // extract solutions and decide solvability
        xx = akku >> 16;
        yy = (akku >> 8) & 0xff;
        xy = (akku >> 0) & 0xff;

        frb = 3.0f * 31.0f / 255.0f / (xx * yy - xy * xy);
        fg = frb * 63.0f / 31.0f;

        // solve.
        max16 = stb__sclamp((At1_r * yy - At2_r * xy) * frb + 0.5f, 0, 31) << 11;
        max16 |= stb__sclamp((At1_g * yy - At2_g * xy) * fg + 0.5f, 0, 63) << 5;
        max16 |= stb__sclamp((At1_b * yy - At2_b * xy) * frb + 0.5f, 0, 31) << 0;

        min16 = stb__sclamp((At2_r * xx - At1_r * xy) * frb + 0.5f, 0, 31) << 11;
        min16 |= stb__sclamp((At2_g * xx - At1_g * xy) * fg + 0.5f, 0, 63) << 5;
        min16 |= stb__sclamp((At2_b * xx - At1_b * xy) * frb + 0.5f, 0, 31) << 0;
    }

    *pmin16 = min16;
    *pmax16 = max16;
    return oldMin != min16 || oldMax != max16;
}

// Color block compression
static void stb__CompressColorBlock(unsigned char *dest, unsigned char *block, int mode)
{
   unsigned int mask;
   int i;
   int refinecount;
   unsigned short max16, min16;
   unsigned char dblock[16*4],color[4*4];
   
   refinecount = (mode & STB_DXT_HIGHQUAL) ? 2 : 1;

   // check if block is constant
   for (i = 1; i < 16; i++)
      if (((unsigned int *) block)[i] != ((unsigned int *) block)[0])
         break;

   if(i == 16) 
   { // constant color
      int r = block[0], g = block[1], b = block[2];
      mask  = 0xaaaaaaaa;
      max16 = (stb__OMatch5[r][0]<<11) | (stb__OMatch6[g][0]<<5) | stb__OMatch5[b][0];
      min16 = (stb__OMatch5[r][1]<<11) | (stb__OMatch6[g][1]<<5) | stb__OMatch5[b][1];
   } else 
   {
      // second step: pca+map along principal axis
      stb__OptimizeColorsBlock(block,&max16,&min16);
      if (max16 != min16) 
	  {
         stb__EvalColors(color,max16,min16);
         mask = stb__MatchColorsBlock(block,color);
      } else
         mask = 0;

      // third step: refine (multiple times if requested)
      for (i=0;i<refinecount;i++) {
         unsigned int lastmask = mask;
         
         if (stb__RefineBlock(block, &max16, &min16, mask)) 
		 {
            if (max16 != min16) 
			{
               stb__EvalColors(color,max16,min16);
               mask = stb__MatchColorsBlock(block, color);
            } else 
			{
               mask = 0;
               break;
            }
         }
         
         if(mask == lastmask)
            break;
      }
  }

  // write the color block
  if(max16 < min16)
  {
     unsigned short t = min16;
     min16 = max16;
     max16 = t;
     mask ^= 0x55555555;
  }

  dest[0] = (unsigned char) (max16);
  dest[1] = (unsigned char) (max16 >> 8);
  dest[2] = (unsigned char) (min16);
  dest[3] = (unsigned char) (min16 >> 8);
  dest[4] = (unsigned char) (mask);
  dest[5] = (unsigned char) (mask >> 8);
  dest[6] = (unsigned char) (mask >> 16);
  dest[7] = (unsigned char) (mask >> 24);
}

static void stb__InitDXT()
{
   int i;
   for(i=0;i<32;i++)
      stb__Expand5[i] = (i<<3)|(i>>2);

   for(i=0;i<64;i++)
      stb__Expand6[i] = (i<<2)|(i>>4);

   for(i=0;i<256+16;i++)
   {
      int v = i-8 < 0 ? 0 : i-8 > 255 ? 255 : i-8;
      stb__QuantRBTab[i] = stb__Expand5[stb__Mul8Bit(v,31)];
      stb__QuantGTab[i] = stb__Expand6[stb__Mul8Bit(v,63)];
   }

   stb__PrepareOptTable(&stb__OMatch5[0][0],stb__Expand5,32);
   stb__PrepareOptTable(&stb__OMatch6[0][0],stb__Expand6,64);
}

void stb_compress_dxt_block(unsigned char *dest, const unsigned char *src, int mode)
{
   static int init=1;
   if (init) 
   {
      stb__InitDXT();
      init=0;
   }

   stb__CompressColorBlock(dest,(unsigned char*) src,mode);
}

inline int imin(int x, int y) { return (x < y) ? x : y; }

static void extractBlock(const unsigned char *src, int x, int y, int w, int h, unsigned char *block)
{
   int i, j;

#ifdef NEW_OPTIMISATIONS
   if ((w-x >=4) && (h-y >=4))
   {
       // Full Square shortcut
       src += x * 4;
       src += y * w * 4;
       for (i = 0; i < 4; ++i)
       {
           // Use SIMD intrinsics for better performance
           _mm_storeu_si128((__m128i*)block, _mm_loadu_si128((__m128i*)src));
           block += sizeof(__m128i); // Move to the next block in the destination
           src += w * 4;
       }
	   return;
   }
#endif

   const int bw = imin(w - x, 4);
   const int bh = imin(h - y, 4);
   const int w4 = w * 4;

   const int remBh = bh * 4;
   const int remBw = bw * 4;

   const int rem[] =
   {
      0, 0, 0, 0,
      0, 1, 0, 1,
      0, 1, 2, 0,
      0, 1, 2, 3
   };

   for (int i = 0; i < 4; ++i)
   {
       const int by = remBh + rem[i] + y;
       const int i44 = i * 4 * 4;
       const int byw4 = by * w4;

       for (int j = 0; j < 4; ++j)
       {
           const int bx = remBw + rem[j] + x;
           const int bx4 = bx * 4;
           const int srcIndex = byw4 + bx4;
           const int index = i44 + j * 4;

           block[index + 0] = src[srcIndex + 0];
           block[index + 1] = src[srcIndex + 1];
           block[index + 2] = src[srcIndex + 2];
           block[index + 3] = src[srcIndex + 3];
       }
   }
}


void rygCompress(unsigned char* dst, const unsigned char* src, int w, int h)
{
    unsigned char block[64];
    int x, y;

    for (y = 0; y < h; y += 4)
    {
        for (x = 0; x < w; x += 4)
        {
            extractBlock(src, x, y, w, h, block);
            stb_compress_dxt_block(dst, block, 10);
            dst += 8;
        }
    }
}

#endif // STB_DXT_IMPLEMENTATION

#endif // STB_INCLUDE_STB_DXT_H