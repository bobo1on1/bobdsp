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

#include <string.h>
#include "noisemeterdetect.h"
#include "util/misc.h"

using namespace BobDSPLadspa;

enum EDETECTTYPE
{
  NONE   = -1,
  ITU468 = 0,
  RMS    = 1,
  AVG    = 2
};

CNoiseMeterDetect::CNoiseMeterDetect(unsigned long samplerate)
{
  memset(m_ports, 0, sizeof(m_ports));
  m_samplerate = samplerate;
  m_type = NONE;
  m_slow = -1;
}

CNoiseMeterDetect::~CNoiseMeterDetect()
{
}

void CNoiseMeterDetect::ConnectPort(unsigned long port, LADSPA_Data* datalocation)
{
  m_ports[port] = datalocation;
}

void CNoiseMeterDetect::Activate()
{
}

void CNoiseMeterDetect::Run(unsigned long samplecount)
{
  InitFilter();

  LADSPA_Data* in = m_ports[0];
  LADSPA_Data* inend = in + samplecount;
  LADSPA_Data* out = m_ports[1];

  if (m_type == ITU468)
  {
    while (in != inend)
    {
      m_itu468detect.process(1, in++);
      *(out++) = m_itu468detect.value();
    }
  }
  else if (m_type == RMS)
  {
    while (in != inend)
    {
      m_rmsdetect.process(1, in++);
      *(out++) = m_rmsdetect.value();
    }
  }
  else if (m_type == AVG)
  {
    while (in != inend)
    {
      m_vumdetect.process(1, in++);
      *(out++) = m_vumdetect.value();
    }
  }
}

void CNoiseMeterDetect::Deactivate()
{
}

void CNoiseMeterDetect::InitFilter()
{
  int32_t type = Clamp((int32_t)ITU468, Round32(*(m_ports[2])), (int32_t)AVG);

  if (type != m_type)
  {
    m_type = type;

    if (m_type == ITU468)
      m_itu468detect.init(m_samplerate);
    else if (m_type == RMS)
      m_rmsdetect.init(m_samplerate);
    else if (m_type == AVG)
      m_vumdetect.init(m_samplerate);
  }

  int32_t slow = Clamp(0, Round32(*(m_ports[3])), 1);

  if (slow != m_slow)
  {
    m_slow = slow;
    if (m_type == RMS)
      m_rmsdetect.speed(m_slow);
    else if (m_type == AVG)
      m_vumdetect.speed(m_slow);
  }
}

