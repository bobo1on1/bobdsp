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
#define INLINE inline __attribute__((always_inline))

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
  memset(&m_indelay, 0, sizeof(m_indelay));
  memset(&m_outdelay, 0, sizeof(m_outdelay));
#else
  memset(m_indelay, 0, sizeof(m_indelay));
  memset(m_outdelay, 0, sizeof(m_outdelay));
  m_delay1 = 0;
  m_delay2 = 1;
#endif
}

void OPTIMIZE CBiquad::Run(unsigned long samplecount)
{
  //calculate the coeffients on each run, since they might change
  m_coefs.Calculate(m_type, m_samplerate, m_ports);

  LADSPA_Data* in    = m_ports[0];
  LADSPA_Data* inend = in + samplecount;
  LADSPA_Data* out   = m_ports[1];

#ifdef USE_SSE

  //load filter coefficients
  __m128 acoeffs  = _mm_set_ps(0.0f, 0.0f, m_coefs.a2, m_coefs.a1);
  __m128 bcoeffs  = _mm_set_ps(0.0f, m_coefs.b2, m_coefs.b1, m_coefs.b0);
  
  //run the quad filter if the input pointer can be aligned to 16 bytes after processing 0 or more samples
  //and there are 4 or more samples to process after getting 16 byte alignment
  uintptr_t inoffset = (uintptr_t)in & 15;
  if ((inoffset == 0 && samplecount >= 4) || ((inoffset & 3) == 0 && samplecount >= 8 - inoffset / 4))
  {
    //process samples until the input pointer gets 16 byte alignment
    if (inoffset != 0)
      RunSingle(in, in + (4 - inoffset / 4), out, acoeffs, bcoeffs);

    //process 4 samples at a time
    RunQuad(in, in + (samplecount & ~3), out, acoeffs, bcoeffs);
  }

  //run the single filter on the remaining samples
  RunSingle(in, inend, out, acoeffs, bcoeffs);

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
void INLINE OPTIMIZE CBiquad::RunSingle(float*& in, float* inend, float*& out, __m128 acoeffs, __m128 bcoeffs)
{
  while (in != inend)
  {
    //load the input value, so that the vector looks like 0 0 0 i
    __m128 invec = _mm_load_ss(in);

    /*unpack with m_indelay so that:
      invec:     0 0 0 i
      m_indelay: e f g h

      becomes

      invec:     g 0 h i
    */
    invec = _mm_unpacklo_ps(invec, m_indelay);

    /*shuffle invec with m_indelay to make:
      m_indelay: f g h i
      now the values in m_indelay are shifted one to the left
      and the new input sample is added to m_indelay
    */
    m_indelay = _mm_shuffle_ps(invec, m_indelay, 0x94);

    //multiply the input samples with the b coefficients
    invec = _mm_mul_ps(m_indelay, bcoeffs);

    //multiply the delayed output samples with the a coefficients
    __m128 outvec = _mm_mul_ps(m_outdelay, acoeffs);

    //add the input and output samples together
    __m128 addvec = _mm_add_ps(invec, outvec);

    //add the 3 calculated samples to the most right float of outvec
    outvec = _mm_movehl_ps(addvec, addvec);
    outvec = _mm_add_ps(outvec, addvec);
    addvec = _mm_shuffle_ps(addvec, addvec, 0x39);
    outvec = _mm_add_ss(outvec, addvec);

    //store the calculated output value
    _mm_store_ss(out, outvec);

    //shift the values in m_outdelay one to the left
    //then add the value in outvec
    outvec = _mm_unpacklo_ps(outvec, m_outdelay);
    m_outdelay = _mm_shuffle_ps(outvec, m_outdelay, 0x94);

    in++;
    out++;
  }
}

void INLINE OPTIMIZE CBiquad::RunQuad(float*& in, float* inend, float*& out, __m128 acoeffs, __m128 bcoeffs)
{
  //same as RunSingle, but with 4 floats at a time
  //it loads 4 floats at the same time using packed load
  //packed store can't be used since the alignment of the output pointer can't be guaranteed to be 16 bytes
  //for some reason it's also slower than a single store
  while (in != inend)
  {
    //do a packed load from a 16 byte aligned pointer
    __m128 inload = _mm_load_ps(in);
    in += 4;

    for (int i = 0; i < 4; i++)
    {
      /*unpack with m_indelay so that:
        inload     0 0 0 i
        m_indelay: e f g h

        becomes

        invec:     g 0 h i
      */
      __m128 invec = _mm_unpacklo_ps(inload, m_indelay);

      /*shuffle invec with m_indelay to make:
        m_indelay: f g h i
        now the values in m_indelay are shifted one to the left
        and the new input sample is added to m_indelay
      */
      m_indelay = _mm_shuffle_ps(invec, m_indelay, 0x94);

      //multiply the input samples with the b coefficients
      invec = _mm_mul_ps(m_indelay, bcoeffs);

      //multiply the delayed output samples with the a coefficients
      __m128 outvec = _mm_mul_ps(m_outdelay, acoeffs);

      //add the input and output samples together
      __m128 addvec = _mm_add_ps(invec, outvec);

      //add the 3 calculated samples to the most right float of outvec
      outvec = _mm_movehl_ps(addvec, addvec);
      outvec = _mm_add_ps(outvec, addvec);
      addvec = _mm_shuffle_ps(addvec, addvec, 0x39);
      outvec = _mm_add_ss(outvec, addvec);

      //store the calculated output value
      _mm_store_ss(out++, outvec);

      //shift the values in m_outdelay one to the left
      //then add the value in outvec
      outvec = _mm_unpacklo_ps(outvec, m_outdelay);
      m_outdelay = _mm_shuffle_ps(outvec, m_outdelay, 0x94);

      //rotate inload to use the next sample
      inload = _mm_shuffle_ps(inload, inload, 0x39);
    }
  }
}
#endif

void CBiquad::Deactivate()
{
}

