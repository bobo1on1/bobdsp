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

#include "floatbufferops.h"

//enable O3 optims to use SSE vector instructions
//for some reason #pragma GCC optimize("O3") has no effect here, dunno why
#define OPTIMIZE __attribute__((optimize("-O3")))\
                 __attribute__((optimize("-ffast-math")))


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

  while (inptr != inend)
    *(outptr++) = *(inptr++) * gain;
}

void OPTIMIZE AvgSquare(float* data, int samples, float& avg)
{
  float* dataptr = data;
  float* end     = dataptr + samples;

  while (dataptr != end)
  {
    avg += *dataptr * *dataptr;
    dataptr++;
  }
}

void OPTIMIZE AvgAbs(float* data, int samples, float& avg)
{
  float* dataptr = data;
  float* end     = dataptr + samples;

  while (dataptr != end)
    avg += __builtin_fabs(*(dataptr++));
}

void OPTIMIZE HighestAbs(float* data, int samples, float& value)
{
  float* dataptr = data;
  float* end     = dataptr + samples;

  while (dataptr != end)
  {
    float absval = __builtin_fabs(*(dataptr++));
    if (absval > value)
      value = absval;
  }
}
