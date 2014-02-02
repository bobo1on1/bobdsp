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
#include "noisemeterweighting.h"
#include "util/misc.h"

using namespace BobDSPLadspa;

enum EWEIGHTINGTYPE
{
  NONE = -1,
  FLAT,
  LOWPASS,
  A_WEIGHTING,
  C_WEIGHTING,
  ITU_R468,
  ITU_R468_DOLBY,
};

CNoiseMeterWeighting::CNoiseMeterWeighting(unsigned long samplerate)
{
  memset(m_ports, 0, sizeof(m_ports));
  m_samplerate = samplerate;
  m_type = NONE;
}

CNoiseMeterWeighting::~CNoiseMeterWeighting()
{
}

void CNoiseMeterWeighting::ConnectPort(unsigned long port, LADSPA_Data* datalocation)
{
  m_ports[port] = datalocation;
}

void CNoiseMeterWeighting::Activate()
{
}

void CNoiseMeterWeighting::Run(unsigned long samplecount)
{
  InitFilter();

  if (m_type == FLAT)
    memcpy(m_ports[1], m_ports[0], samplecount * sizeof(LADSPA_Data));
  else if (m_type == LOWPASS)
    m_lpeq20filter.process(samplecount, m_ports[0], m_ports[1]);
  else if (m_type == A_WEIGHTING)
    m_acfilter.process(samplecount, m_ports[0], m_ports[1], NULL);
  else if (m_type == C_WEIGHTING)
    m_acfilter.process(samplecount, m_ports[0], NULL, m_ports[1]);
  else if (m_type == ITU_R468 || m_type == ITU_R468_DOLBY)
    m_itu468filter.process(samplecount, m_ports[0], m_ports[1]);
}

void CNoiseMeterWeighting::Deactivate()
{
}

void CNoiseMeterWeighting::InitFilter()
{
  int32_t type = Clamp((int32_t)FLAT, Round32(*(m_ports[2])), (int32_t)ITU_R468_DOLBY);

  if (type != m_type)
  {
    m_type = type;

    if (m_type == LOWPASS)
      m_lpeq20filter.init(m_samplerate);
    else if (m_type == A_WEIGHTING || m_type == C_WEIGHTING)
      m_acfilter.init(m_samplerate);
    else if (m_type == ITU_R468 || m_type == ITU_R468_DOLBY)
      m_itu468filter.init(m_samplerate, m_type == ITU_R468_DOLBY);
  }
}

