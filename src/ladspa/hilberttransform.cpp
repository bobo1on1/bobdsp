/*
 * bobdsp
 * Copyright (C) Bob 2017
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

#include "hilberttransform.h"
#include "util/inclstdint.h"
#include "util/ssedefs.h"
#include <string.h>
#include <stdlib.h>

#define GAIN 1.570483967e+00

//coefficients generated using http://www-users.cs.york.ac.uk/~fisher/mkfilter/hilbert.html
//this causes a 90 degree phase lag for all frequencies, it also acts as a high pass filter with the -3db point at about 70 hertz with a sample rate of 48 khz
//the filter delay is 256 samples, so it will only have the 90 degree phase shift compared to a signal with the same delay

static const float __attribute__ ((aligned(ALIGN))) g_coeffs[BUFSIZE] =
{
  +0.0003138613, +0.0003174376,
  +0.0003221740, +0.0003280972,
  +0.0003352340, +0.0003436114,
  +0.0003532566, +0.0003641970,
  +0.0003764603, +0.0003900744,
  +0.0004050673, +0.0004214676,
  +0.0004393040, +0.0004586054,
  +0.0004794011, +0.0005017209,
  +0.0005255947, +0.0005510531,
  +0.0005781267, +0.0006068469,
  +0.0006372454, +0.0006693544,
  +0.0007032067, +0.0007388356,
  +0.0007762750, +0.0008155595,
  +0.0008567243, +0.0008998053,
  +0.0009448395, +0.0009918643,
  +0.0010409181, +0.0010920405,
  +0.0011452719, +0.0012006537,
  +0.0012582286, +0.0013180405,
  +0.0013801346, +0.0014445576,
  +0.0015113575, +0.0015805840,
  +0.0016522886, +0.0017265245,
  +0.0018033470, +0.0018828134,
  +0.0019649832, +0.0020499184,
  +0.0021376834, +0.0022283457,
  +0.0023219754, +0.0024186459,
  +0.0025184340, +0.0026214200,
  +0.0027276884, +0.0028373275,
  +0.0029504304, +0.0030670949,
  +0.0031874241, +0.0033115266,
  +0.0034395171, +0.0035715167,
  +0.0037076536, +0.0038480636,
  +0.0039928907, +0.0041422875,
  +0.0042964166, +0.0044554506,
  +0.0046195735, +0.0047889815,
  +0.0049638842, +0.0051445053,
  +0.0053310847, +0.0055238792,
  +0.0057231643, +0.0059292361,
  +0.0061424131, +0.0063630386,
  +0.0065914829, +0.0068281459,
  +0.0070734605, +0.0073278960,
  +0.0075919615, +0.0078662107,
  +0.0081512469, +0.0084477282,
  +0.0087563740, +0.0090779724,
  +0.0094133886, +0.0097635740,
  +0.0101295776, +0.0105125586,
  +0.0109138011, +0.0113347310,
  +0.0117769368, +0.0122421921,
  +0.0127324845, +0.0132500473,
  +0.0137973997, +0.0143773929,
  +0.0149932668, +0.0156487183,
  +0.0163479841, +0.0170959429,
  +0.0178982414, +0.0187614506,
  +0.0196932631, +0.0207027420,
  +0.0218006399, +0.0229998108,
  +0.0243157494, +0.0257673042,
  +0.0273776363, +0.0291755264,
  +0.0311971870, +0.0334888257,
  +0.0361103470, +0.0391408311,
  +0.0426868713, +0.0468956751,
  +0.0519764409, +0.0582368230,
  +0.0661485683, +0.0764737425,
  +0.0905286557, +0.1107996896,
  +0.1426148288, +0.1998268664,
  +0.3332294323, +0.9999653629,
  -0.9999653629, -0.3332294323,
  -0.1998268664, -0.1426148288,
  -0.1107996896, -0.0905286557,
  -0.0764737425, -0.0661485683,
  -0.0582368230, -0.0519764409,
  -0.0468956751, -0.0426868713,
  -0.0391408311, -0.0361103470,
  -0.0334888257, -0.0311971870,
  -0.0291755264, -0.0273776363,
  -0.0257673042, -0.0243157494,
  -0.0229998108, -0.0218006399,
  -0.0207027420, -0.0196932631,
  -0.0187614506, -0.0178982414,
  -0.0170959429, -0.0163479841,
  -0.0156487183, -0.0149932668,
  -0.0143773929, -0.0137973997,
  -0.0132500473, -0.0127324845,
  -0.0122421921, -0.0117769368,
  -0.0113347310, -0.0109138011,
  -0.0105125586, -0.0101295776,
  -0.0097635740, -0.0094133886,
  -0.0090779724, -0.0087563740,
  -0.0084477282, -0.0081512469,
  -0.0078662107, -0.0075919615,
  -0.0073278960, -0.0070734605,
  -0.0068281459, -0.0065914829,
  -0.0063630386, -0.0061424131,
  -0.0059292361, -0.0057231643,
  -0.0055238792, -0.0053310847,
  -0.0051445053, -0.0049638842,
  -0.0047889815, -0.0046195735,
  -0.0044554506, -0.0042964166,
  -0.0041422875, -0.0039928907,
  -0.0038480636, -0.0037076536,
  -0.0035715167, -0.0034395171,
  -0.0033115266, -0.0031874241,
  -0.0030670949, -0.0029504304,
  -0.0028373275, -0.0027276884,
  -0.0026214200, -0.0025184340,
  -0.0024186459, -0.0023219754,
  -0.0022283457, -0.0021376834,
  -0.0020499184, -0.0019649832,
  -0.0018828134, -0.0018033470,
  -0.0017265245, -0.0016522886,
  -0.0015805840, -0.0015113575,
  -0.0014445576, -0.0013801346,
  -0.0013180405, -0.0012582286,
  -0.0012006537, -0.0011452719,
  -0.0010920405, -0.0010409181,
  -0.0009918643, -0.0009448395,
  -0.0008998053, -0.0008567243,
  -0.0008155595, -0.0007762750,
  -0.0007388356, -0.0007032067,
  -0.0006693544, -0.0006372454,
  -0.0006068469, -0.0005781267,
  -0.0005510531, -0.0005255947,
  -0.0005017209, -0.0004794011,
  -0.0004586054, -0.0004393040,
  -0.0004214676, -0.0004050673,
  -0.0003900744, -0.0003764603,
  -0.0003641970, -0.0003532566,
  -0.0003436114, -0.0003352340,
  -0.0003280972, -0.0003221740,
  -0.0003174376, -0.0003138613,
};

using namespace BobDSPLadspa;

CHilbertTransform::CHilbertTransform()
{
  m_bufindex = 0;
  for (int i = 0; i < 2; i++)
  {
    m_buf[i] = (float*)aligned_alloc(ALIGN, BUFSIZE * sizeof(float));
    memset(m_buf[i], 0, BUFSIZE * sizeof(float));
  }
}

CHilbertTransform::~CHilbertTransform()
{
  for (int i = 0; i < 2; i++)
    free(m_buf[i]);
}

void CHilbertTransform::Reset()
{
  for (int i = 0; i < 2; i++)
    memset(m_buf[i], 0, BUFSIZE * sizeof(float));
}

float OPTIMIZE CHilbertTransform::Process(float in)
{
  //move all samples back one position, and store the new sample
  memmove(m_buf[m_bufindex], m_buf[m_bufindex] + 1, (BUFSIZE - 1) * sizeof(float));
  m_buf[m_bufindex][BUFSIZE - 1] = in / GAIN;

  //change the buffer for the current filter run
  m_bufindex = !m_bufindex;

  //pick the buffer to use, because the FIR filter only uses every other sample
  //the input samples are stored across two buffers
  float* bufptr = m_buf[m_bufindex];
  float* bufend = bufptr + BUFSIZE;
  const float* coeffsptr = g_coeffs;

  float sum;
#ifdef USE_SSE
  ssevec sumvec;
  sumvec.v = _mm_setzero_ps();

  ssevec samples;
  ssevec coeffs;
  while (bufptr != bufend)
  {
    samples.v = _mm_load_ps(bufptr);    //load 4 samples
    coeffs.v  = _mm_load_ps(coeffsptr); //load 4 coefficients

    //multiply the 4 samples with the 4 coefficients, and add them to sumvec
    sumvec.v  = _mm_add_ps(sumvec.v, _mm_mul_ps(samples.v, coeffs.v));

    bufptr    += 4;
    coeffsptr += 4;
  }

  sum = sumvec.f[0] + sumvec.f[1] + sumvec.f[2] + sumvec.f[3];
#else
  //apply the FIR filter
  sum = 0.0f;
  while (bufptr != bufend)
    sum += *(coeffsptr++) * *(bufptr++);
#endif

  return sum;
}

