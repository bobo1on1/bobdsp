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

#include <string.h>
#include <stdlib.h>
#include "biquad.h"

using namespace BobDSPLadspa;

//optim, force inlining because of reference variables
#define INLINE __attribute__((always_inline))

CBiquad::CBiquad(EFILTER type, unsigned long samplerate)
{
  memset(m_ports, 0, sizeof(m_ports));
  m_type = type;
  m_samplerate = samplerate;
}

CBiquad::~CBiquad()
{
}

void CBiquad::ConnectPort(unsigned long port, LADSPA_Data* datalocation)
{
  m_ports[port] = datalocation;
}

void CBiquad::Activate()
{
#ifdef USE_SSE
  posix_memalign((void**)&m_indelay, 16, 4 * sizeof(float));
  posix_memalign((void**)&m_outdelay, 16, 4 * sizeof(float));
  memset(m_indelay, 0, 4 * sizeof(float));
  memset(m_outdelay, 0, 4 * sizeof(float));
#else
  memset(m_indelay, 0, sizeof(m_indelay));
  memset(m_outdelay, 0, sizeof(m_outdelay));
  m_delay1 = 0;
  m_delay2 = 1;
#endif

  m_coefs.Calculate(m_type, m_samplerate, m_ports, true);
}

void OPTIMIZE CBiquad::Run(unsigned long samplecount)
{
  //calculate the coeffients on each run, since they might change
  m_coefs.Calculate(m_type, m_samplerate, m_ports, false);

  LADSPA_Data* in    = m_ports[0];
  LADSPA_Data* inend = in + samplecount;
  LADSPA_Data* out   = m_ports[1];

#ifdef USE_SSE

  //load delays from a 16 byte aligned pointer
  __m128 indelay  = _mm_load_ps(m_indelay);
  __m128 outdelay = _mm_load_ps(m_outdelay);

  //load filter coefficients
  __m128 acoeffs  = _mm_set_ps(0.0f, 0.0f, m_coefs.a2, m_coefs.a1);
  __m128 bcoeffs  = _mm_set_ps(0.0f, m_coefs.b2, m_coefs.b1, m_coefs.b0);
  
  //run the quad filter
  if (samplecount >= 4)
    RunQuad(in, in + (samplecount & ~3), out, indelay, outdelay, acoeffs, bcoeffs);

  //run the single filter on the remaining samples
  RunSingle(in, inend, out, indelay, outdelay, acoeffs, bcoeffs);

  //store the delays for the next run
  _mm_store_ps(m_indelay, indelay);
  _mm_store_ps(m_outdelay, outdelay);

#else

  //run a standard biquad filter
  while (in != inend)
  {
    *out = *in * m_coefs.b0 +
           m_indelay[m_delay1] * m_coefs.b1 +
           m_indelay[m_delay2] * m_coefs.b2 +
           m_outdelay[m_delay1] * m_coefs.a1 +
           m_outdelay[m_delay2] * m_coefs.a2;

    m_indelay[m_delay2] = *in;
    m_outdelay[m_delay2] = *out;

    m_delay1 ^= 1;
    m_delay2 ^= 1;

    in++;
    out++;
  }
#endif
}

#ifdef USE_SSE
void INLINE OPTIMIZE CBiquad::RunSingle(float*& in, float* inend, float*& out, __m128& indelay,
                                        __m128& outdelay, __m128& acoeffs, __m128& bcoeffs)
{
  while (in != inend)
  {
    //load the input value, so that the vector looks like 0 0 0 i
    __m128 invec = _mm_load_ss(in);

    /*unpack with indelay so that:
      invec:     0 0 0 i
      ndelay:    e f g h

      becomes

      invec:     g 0 h i
    */
    invec = _mm_unpacklo_ps(invec, indelay);

    /*shuffle invec with indelay to make:
      indelay: f g h i
      now the values in indelay are shifted one to the left
      and the new input sample is added to indelay
    */
    indelay = _mm_shuffle_ps(invec, indelay, 0x94);

    //multiply the input samples with the b coefficients
    invec = _mm_mul_ps(indelay, bcoeffs);

    //multiply the delayed output samples with the a coefficients
    __m128 outvec = _mm_mul_ps(outdelay, acoeffs);

    //add the input and output samples together
    __m128 addvec = _mm_add_ps(invec, outvec);

    //add the 3 calculated samples to the most right float of outvec
    outvec = addvec;
    addvec = _mm_shuffle_ps(addvec, addvec, 0x39);
    outvec = _mm_add_ss(outvec, addvec);
    addvec = _mm_shuffle_ps(addvec, addvec, 0x39);
    outvec = _mm_add_ss(outvec, addvec);

    //store the calculated output value
    _mm_store_ss(out, outvec);

    //shift the values in outdelay one to the left
    //then add the value in outvec
    outvec = _mm_unpacklo_ps(outvec, outdelay);
    outdelay = _mm_shuffle_ps(outvec, outdelay, 0x94);

    in++;
    out++;
  }
}

void INLINE OPTIMIZE CBiquad::RunQuad(float*& in, float* inend, float*& out, __m128& indelay,
                                      __m128& outdelay, __m128& acoeffs, __m128& bcoeffs)
{
  //same as RunSingle, but with 4 floats at a time
  while (in != inend)
  {
    //load in 4 input floats from a possibly unaligned pointer
    //using aligned load here makes no difference in performance
    __m128 inload = _mm_loadu_ps(in);

    //calculate 4 output values
    for (int i = 0; i < 4; i++)
    {
      //rotate to use the next input value
      inload = _mm_shuffle_ps(inload, inload, 0x93);

      __m128 invec = inload;
      invec = _mm_unpacklo_ps(invec, indelay);
      indelay = _mm_shuffle_ps(invec, indelay, 0x94);
      invec = _mm_mul_ps(indelay, bcoeffs);

      __m128 outvec = _mm_mul_ps(outdelay, acoeffs);
      __m128 addvec = _mm_add_ps(invec, outvec);

      outvec = addvec;
      addvec = _mm_shuffle_ps(addvec, addvec, 0x39);
      outvec = _mm_add_ss(outvec, addvec);
      addvec = _mm_shuffle_ps(addvec, addvec, 0x39);
      outvec = _mm_add_ss(outvec, addvec);

      outvec = _mm_unpacklo_ps(outvec, outdelay);
      outdelay = _mm_shuffle_ps(outvec, outdelay, 0x94);
    }

    //store 4 output floats into an unaligned pointer
    //using an aligned store here makes no difference in performance
    _mm_storeu_ps(out, outdelay);

    in += 4;
    out += 4;
  }
}
#endif

void CBiquad::Deactivate()
{
#ifdef USE_SSE
  free(m_indelay);
  free(m_outdelay);
#endif
}

