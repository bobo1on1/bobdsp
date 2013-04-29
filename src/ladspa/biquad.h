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

#ifndef BIQUAD_H
#define BIQUAD_H

#include "config.h"
#include "util/inclstdint.h"
#include "util/ssedefs.h"
#include "biquadcoefs.h"
#include "filterdescriptions.h"
#include "filterinterface.h"

namespace BobDSPLadspa
{
  class CBiquad : public IFilter
  {
    public:
      CBiquad(EFILTER type, unsigned long samplerate);
      ~CBiquad();

      void ConnectPort(unsigned long port, LADSPA_Data* datalocation);
      void Activate();
      void Run(unsigned long samplecount);
      void Deactivate();

    private:
#ifdef USE_SSE
      void RunSingle(float*& in, float* inend, float*& out, __m128 acoeffs, __m128 bcoeffs);
      void RunQuad(float*& in, float* inend, float*& out, __m128 acoeffs, __m128 bcoeffs);
#endif

      EFILTER      m_type;
      CBiquadCoef  m_coefs;
      float        m_samplerate;
      LADSPA_Data* m_ports[6];

#ifdef USE_SSE
      __m128       m_indelay;
      __m128       m_outdelay;
#else
      LADSPA_Data  m_indelay[2];
      LADSPA_Data  m_outdelay[2];
      int          m_delay1;
      int          m_delay2;
#endif
  };
}

#endif //BIQUAD_H
