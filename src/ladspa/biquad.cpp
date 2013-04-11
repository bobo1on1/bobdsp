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
#include "biquad.h"
#include "util/inclstdint.h"
#include "util/ssedefs.h"

using namespace BobDSPLadspa;

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
  memset(m_indelay, 0, sizeof(m_indelay));
  memset(m_outdelay, 0, sizeof(m_outdelay));
  m_delay1 = 0;
  m_delay2 = 1;

  m_coefs.Calculate(m_type, m_samplerate, m_ports, true);
}

void CBiquad::Run(unsigned long samplecount)
{
  //calculate the coeffients on each run, since they might change
  m_coefs.Calculate(m_type, m_samplerate, m_ports, false);

  LADSPA_Data* in    = m_ports[0];
  LADSPA_Data* inend = in + samplecount;
  LADSPA_Data* out   = m_ports[1];

  //run a standard biquad filter
  while (in != inend)
  {
    *out = (*in * m_coefs.b0 +
           m_indelay[m_delay1] * m_coefs.b1 +
           m_indelay[m_delay2] * m_coefs.b2 +
           m_outdelay[m_delay1] * m_coefs.a1 +
           m_outdelay[m_delay2] * m_coefs.a2) *
           m_coefs.a0;

    m_indelay[m_delay2] = *in;
    m_outdelay[m_delay2] = *out;

    m_delay1 ^= 1;
    m_delay2 ^= 1;

    in++;
    out++;
  }
}

void CBiquad::Deactivate()
{
}

