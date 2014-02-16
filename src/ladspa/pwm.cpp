/*
 * bobdsp
 * Copyright (C) Bob 2014
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

#include "pwm.h"
#include "util/misc.h"
#include <string.h>
#include <math.h>

using namespace BobDSPLadspa;

CPwm::CPwm(unsigned long samplerate)
{
  m_samplerate = samplerate;
  memset(m_ports, 0, sizeof(m_ports));
  m_state = 0.0f;
  m_samplecounter = 0;
  m_accumulator = 0.0;
  m_accumsamples = 0;
  m_outval = 0;
}

CPwm::~CPwm()
{
}

void CPwm::ConnectPort(unsigned long port, LADSPA_Data* datalocation)
{
  m_ports[port] = datalocation;
}

void CPwm::Activate()
{
  m_state = 1.0f;
  m_samplecounter = 0;
  m_accumulator = 0.0;
  m_accumsamples = 0;
  m_outval = 0;
}

void CPwm::Run(unsigned long samplecount)
{
  LADSPA_Data* in = m_ports[0];
  LADSPA_Data* inend = in + samplecount;
  LADSPA_Data* out = m_ports[1];

  int32_t period = Clamp(Round32(*(m_ports[2])), 1, 0xFFFFFFF);

  while (in != inend)
  {
    if (m_samplecounter < m_outval)
      *out = m_state;
    else
      *out = 0.0f;

    m_accumulator += fabs(*in);
    m_accumsamples++;
    m_samplecounter++;
    if (m_samplecounter >= period)
    {
      m_samplecounter = 0;
      m_state *= -1.0f;
      m_outval = Round32((m_accumulator / m_accumsamples) * (double)period);
      m_accumulator = 0.0;
      m_accumsamples = 0;
    }

    in++;
    out++;
  }
}

void CPwm::Deactivate()
{
}

