/*
 * bobdsp
 * Copyright (C) Bob 2012
 * 
 * bobdsp is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * bobdsp is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "inclstdint.h"
#include <float.h>

#include "floatbufferops.h"
#include "ssedefs.h"

#ifdef USE_SSE

static bool IsAligned(float* ptr, int samples, float*& leadinend)
{
  uintptr_t offset = (uintptr_t)ptr & (ALIGN - 1);
  unsigned long bytes = samples * sizeof(float);
  //check if the pointer is aligned to 16 bytes, and there are at least 16 bytes to process
  if (offset == 0 && bytes >= ALIGN)
  {
    leadinend = ptr;
    return true;
  }
  //check if the pointer is aligned to 4 bytes, then it's possible to get 16 byte alignment
  //after at most 3 iterations, then check if there are at least 16 bytes to process after that
  else if ((offset & 3) == 0 && bytes - (ALIGN - offset) >= ALIGN)
  {
    leadinend = (float*)(((uintptr_t)ptr & (~(ALIGN - 1))) + ALIGN);
    return true;
  }
  //no good alignment, don't use SSE
  else
  {
    return false;
  }
}

inline float* VecEnd(float* endptr)
{
  return (float*)((uintptr_t)endptr & ~(ALIGN - 1));
}

#endif

//the gcc vectorizer can handle this
void OPTIMIZE ApplyGain(float* data, int samples, float gain) 
{
  float* dataptr = data;
  float* end     = dataptr + samples;

  while (dataptr != end)
    *(dataptr++) *= gain;
}

void OPTIMIZE CopyApplyGain(float* in, float* out, int samples, float gain)
{
  float* inptr  = in;
  float* inend  = inptr + samples;
  float* outptr = out;

#ifdef USE_SSE
  //check if both pointers are aligned to 16 bytes,
  //or if both pointers are aligned to 4 bytes
  //and have the same alignment offset from 16
  //and if there are enough samples to do at least one SSE pass
  unsigned long bytes = samples * sizeof(float);
  uintptr_t inoffset = (uintptr_t)inptr & (ALIGN - 1);
  uintptr_t outoffset = (uintptr_t)outptr & (ALIGN - 1);
  if ((inoffset == 0 && outoffset == 0 && bytes >= 16) ||
      ((inoffset & 3) == 0 && inoffset == outoffset && bytes - (ALIGN - inoffset) >= ALIGN))
  {
    //process any samples up to 16 byte alignment
    if (inoffset != 0)
    {
      float* leadinend = (float*)(((uintptr_t)inptr & ~(ALIGN - 1)) + ALIGN);
      while (inptr != leadinend)
        *(outptr++) = *(inptr++) * gain;
    }

    //create a float vector to multiply with
    ssevec vecgain;
    for (int i = 0; i < 4; i++)
      vecgain.f[i] = gain;

    //get 4 floats from the aligned inptr, multiply with gain,
    //then store them into the aligned outptr
    __m128 vecin;
    __m128 vecout;
    float* vecend = VecEnd(inend);
    while (inptr != vecend)
    {
      vecin = _mm_load_ps(inptr);
      vecout = _mm_mul_ps(vecin, vecgain.v);
      _mm_store_ps(outptr, vecout);
      inptr += 4;
      outptr += 4;
    }
  }
#endif

  //process any remaining samples
  //the gcc vectorizer will also handle SSE with unaligned pointers here
  while (inptr != inend)
    *(outptr++) = *(inptr++) * gain;
}

void OPTIMIZE DenormalsToZero(float* data, int samples)
{
  float* dataptr = data;
  float* end     = dataptr + samples;

#ifdef USE_SSE
  //AND mask to set the sign bit to 0
  ssevec absmask;
  for (int i = 0; i < 4; i++)
    absmask.i[i] = 0x7FFFFFFF;

  //value to compare to, lower than this is a denormal
  ssevec cmpval;
  for (int i = 0; i < 4; i++)
    cmpval.f[i] = FLT_MIN;

  float* leadinend;
  if (IsAligned(data, samples, leadinend))
  {
    //process until dataptr is 16 bytes aligned
    while (dataptr != leadinend)
    {
      if (__builtin_fabs(*dataptr) < FLT_MIN)
        *dataptr = 0.0f;

      dataptr++;
    }

    __m128 sample;
    __m128 result;
    float* vecend = VecEnd(end);
    while (dataptr != vecend)
    {
      //load from a 16 byte aligned pointer
      sample = _mm_load_ps(dataptr);
      //set sign bit to zero to get the absolute value
      result = _mm_and_ps(sample, absmask.v);

      //create an AND mask with all ones if the value is equal or greater than FLT_MIN
      //if it's lower create an AND mask of all zeros
      result = _mm_cmpge_ps(result, cmpval.v);

      //apply the AND mask, and store the values back into the 16 byte aligned pointer
      sample = _mm_and_ps(sample, result);
      _mm_store_ps(dataptr, sample);

      dataptr += 4;
    }
  }
#endif

  //process remaining values
  while (dataptr != end)
  {
    if (__builtin_fabs(*dataptr) < FLT_MIN)
      *dataptr = 0.0f;

    dataptr++;
  }
}

static void OPTIMIZE ProcessFloatStats(float* data, int samples, float& output,
                                       void(*ProcessFunc)(float*& dataptr, float* end, float& output),
                                       void(*ProcessFuncSSE)(float*& dataptr, float* end, float& output))
{
  float* dataptr = data;
  float* end     = dataptr + samples;

#ifdef USE_SSE
  float* leadinend;
  if (IsAligned(data, samples, leadinend))
  {
    //process floats until dataptr is aligned to 16 bytes
    ProcessFunc(dataptr, leadinend, output);

    //process 4 floats at a time with SSE until there are less than 4 floats to process
    ProcessFuncSSE(dataptr, VecEnd(end), output);
  }
#endif

  //process any remaining floats
  ProcessFunc(dataptr, end, output);
}

static void OPTIMIZE AvgSquareProcess(float*& dataptr, float* end, float& output)
{
  //optim, increasing a local pointer is faster than doing it via a reference variable
  float* privdataptr = dataptr;

  while (privdataptr != end)
  {
    output += *privdataptr * *privdataptr;
    privdataptr++;
  }

  dataptr = privdataptr;
}

static void OPTIMIZE AvgSquareProcessSSE(float*& dataptr, float* end, float& output)
{
#ifdef USE_SSE
  float* privdataptr = dataptr;

  //place to store averages from SSE
  ssevec avgs;
  avgs.v = _mm_setzero_ps();

  //process 4 floats at a time with SSE until
  //there are less than 4 floats to process
  __m128 vecdata;
  while (privdataptr != end)
  {
    //load 4 floats from an aligned float* into vecdata
    vecdata = _mm_load_ps(privdataptr);
    //multiply vecdata with itself and add the result to avgs
    avgs.v = _mm_add_ps(avgs.v, _mm_mul_ps(vecdata, vecdata));
    privdataptr += 4;
  }

  //add the 4 averages from SSE to the average
  for (int i = 0; i < 4; i++)
    output += avgs.f[i];

  dataptr = privdataptr;
#endif
}

void OPTIMIZE AvgSquare(float* data, int samples, float& avg)
{
  ProcessFloatStats(data, samples, avg, AvgSquareProcess, AvgSquareProcessSSE);
}

static void OPTIMIZE AvgAbsProcess(float*& dataptr, float* end, float& output)
{
  //optim, increasing a local pointer is faster than doing it via a reference variable
  float* privdataptr = dataptr;

  while (privdataptr != end)
    output += __builtin_fabs(*(privdataptr++));

  dataptr = privdataptr;
}

static void OPTIMIZE AvgAbsProcessSSE(float*& dataptr, float* end, float& output)
{
#ifdef USE_SSE
  float* privdataptr = dataptr;

  //place to store values from SSE
  ssevec avgs;
  avgs.v = _mm_setzero_ps();

  //AND mask to set the sign bit to 0
  ssevec absmask;
  for (int i = 0; i < 4; i++)
    absmask.i[i] = 0x7FFFFFFF;

  //process 4 floats at a time with SSE until
  //there are less than 4 floats to process
  __m128 vecdata;
  while (privdataptr != end)
  {
    //load 4 floats from an aligned float* into vecdata
    vecdata = _mm_load_ps(privdataptr);
    //set the sign bit to 0 to get the absolute value, the add the result to avgs
    avgs.v = _mm_add_ps(avgs.v, _mm_and_ps(vecdata, absmask.v));
    privdataptr += 4;
  }

  //add the result from SSE
  for (int i = 0; i < 4; i++)
    output += avgs.f[i];

  dataptr = privdataptr;
#endif
}

void OPTIMIZE AvgAbs(float* data, int samples, float& avg)
{
  ProcessFloatStats(data, samples, avg, AvgAbsProcess, AvgAbsProcessSSE);
}

static void OPTIMIZE HighestAbsProcess(float*& dataptr, float* end, float& output)
{
  //optim, increasing a local pointer is faster than doing it via a reference variable
  float* privdataptr = dataptr;

  while (privdataptr != end)
  {
    float absval = *(privdataptr++);
    if (absval > output)
      absval = output;
  }

  dataptr = privdataptr;
}

static void OPTIMIZE HighestAbsProcessSSE(float*& dataptr, float* end, float& output)
{
#ifdef USE_SSE
  float* privdataptr = dataptr;

  //place to store values from SSE
  ssevec highest;
  highest.v = _mm_setzero_ps();

  //AND mask to set the sign bit to 0
  ssevec absmask;
  for (int i = 0; i < 4; i++)
    absmask.i[i] = 0x7FFFFFFF;

  //process 4 floats at a time with SSE until
  //there are less than 4 floats to process
  __m128 vecdata;
  while (privdataptr != end)
  {
    //load 4 floats from an aligned float* into vecdata
    vecdata = _mm_load_ps(privdataptr);
    //set the sign bit to 0 to get the absolute value, the add the result to avgs
    highest.v = _mm_max_ps(highest.v, _mm_and_ps(vecdata, absmask.v));
    privdataptr += 4;
  }

  //add the result from SSE
  for (int i = 0; i < 4; i++)
  {
    if (highest.f[i] > output)
      output = highest.f[i];
  }

  dataptr = privdataptr;
#endif
}

void OPTIMIZE HighestAbs(float* data, int samples, float& value)
{
  ProcessFloatStats(data, samples, value, HighestAbsProcess, HighestAbsProcessSSE);
}

