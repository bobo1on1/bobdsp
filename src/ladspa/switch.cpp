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
#include <math.h>
#include "switch.h"
#include "util/misc.h"

using namespace BobDSPLadspa;


CSwitch::CSwitch(unsigned long samplerate)
{
  memset(m_ports, 0, sizeof(m_ports));
  memset(m_prevports, 0, sizeof(m_prevports));
  m_samplerate = samplerate;
  m_samplecounter = 0;
  m_level = 0.0f;
  m_turnondelay = 0;
  m_turnoffdelay = 0;
  m_state = 0.0f;
}

CSwitch::~CSwitch()
{
}

void CSwitch::ConnectPort(unsigned long port, LADSPA_Data* datalocation)
{
  m_ports[port] = datalocation;
}

void CSwitch::Activate()
{
  m_samplecounter = 0;
  m_state = 0.0f;
  Setup();
}

void CSwitch::Run(unsigned long samplecount)
{
  for (int i = 0; i < 3; i++)
  {
    if (*(m_ports[i + 3]) != m_prevports[i])
    {
      Setup();
      break;
    }
  }

  LADSPA_Data* in = m_ports[0];
  LADSPA_Data* inend = in + samplecount;
  LADSPA_Data* out = m_ports[1];
  LADSPA_Data* trigger = m_ports[2];

  while(in != inend)
  {
    if (m_state > 0.5f)
    {
      *(trigger++) = 1.0f - ((float)m_samplecounter / (float)m_turnoffdelay);
      if (Abs(*in) < m_level)
      {
        m_samplecounter++;
        if (m_samplecounter >= m_turnoffdelay)
        {
          m_state = 0.0f;
          m_samplecounter = 0;
        }
      }
      else if (m_samplecounter > 0)
      {
        m_samplecounter--;
      }
    }
    else
    {
      *(trigger++) = (float)m_samplecounter / (float)m_turnondelay;
      if (Abs(*in) > m_level)
      {
        m_samplecounter++;
        if (m_samplecounter >= m_turnondelay)
        {
          m_state = 1.0f;
          m_samplecounter = 0;
        }
      }
      else if (m_samplecounter > 0)
      {
        m_samplecounter--;
      }
    }

    in++;
    *(out++) = m_state;
  }
}

void CSwitch::Deactivate()
{
}

void CSwitch::Setup()
{
  for (int i = 0; i < 3; i++)
    m_prevports[i] = *(m_ports[i + 3]);

  m_level = pow(10.0, *(m_ports[3]) / 20.0f);
  m_turnondelay = *(m_ports[4]) * m_samplerate;
  m_turnoffdelay = *(m_ports[5]) * m_samplerate;
}

