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

#include "hilberttransformplugin.h"

#include <string.h>
#include <algorithm>
#include <math.h>

using namespace BobDSPLadspa;

CHilbertTransformPlugin::CHilbertTransformPlugin()
{
  memset(m_ports, 0, sizeof(m_ports));
  memset(m_delaybuf, 0, sizeof(m_delaybuf));
  m_delaybufpos = 0;
}

CHilbertTransformPlugin::~CHilbertTransformPlugin()
{
}

void CHilbertTransformPlugin::ConnectPort(unsigned long port, LADSPA_Data* datalocation)
{
  m_ports[port] = datalocation;
}

void CHilbertTransformPlugin::Activate()
{
  memset(m_delaybuf, 0, sizeof(m_delaybuf));
  m_delaybufpos = 0;
  for (int i = 0; i < HT_NUMCHANNELS; i++)
    m_hilberttransform[i].Reset();
}

void CHilbertTransformPlugin::Run(unsigned long samplecount)
{
  LADSPA_Data dryratio[HT_NUMCHANNELS];
  LADSPA_Data wetratio[HT_NUMCHANNELS] = {*(m_ports[HT_LEFT_DRYWET]), *(m_ports[HT_RIGHT_DRYWET])};
  for (int c = 0; c < HT_NUMCHANNELS; c++)
  {
    wetratio[c] = std::min(std::max(wetratio[c], 0.0f), 1.0f);
    dryratio[c] = 1.0f - wetratio[c];

    //when adding two sines that are shifted by 90 degrees, the amplitude is increased by the square root of 2
    //however the wet/dry balance assumes that the amplitude is increased by 2
    //so take the distance of the wet value to 0.5, and use it to calculate a gain value between the square root of 2 and 1
    LADSPA_Data gain = (1.0f - (fabsf(wetratio[c] - 0.5f) * 2.0f)) * (M_SQRT2 - 1.0f) + 1.0f;
    dryratio[c] *= gain;
    wetratio[c] *= gain;
  }

  LADSPA_Data in[HT_NUMCHANNELS];
  LADSPA_Data out[HT_NUMCHANNELS];
  for (unsigned long i = 0; i < samplecount; i++)
  {
    //read input samples
    in[HT_LEFT_CHAN]  = m_ports[HT_LEFT_IN][i];
    in[HT_RIGHT_CHAN] = m_ports[HT_RIGHT_IN][i];

    for (int c = 0; c < HT_NUMCHANNELS; c++)
    {
      //get the wet sample from the hilbert transform, and the dry sample from the delay buffer
      //the delay buffer is neede because the hilbert transform causes a delay of half its filter length
      out[c] = m_hilberttransform[c].Process(in[c]) * wetratio[c] + m_delaybuf[c][m_delaybufpos] * dryratio[c];

      //write the new input sample into the delay buffer
      m_delaybuf[c][m_delaybufpos] = in[c];
    }

    m_delaybufpos++;
    if (m_delaybufpos >= FILTERSIZE / 2)
      m_delaybufpos = 0;

    //write output samples
    m_ports[HT_LEFT_OUT][i]  = out[HT_LEFT_CHAN];
    m_ports[HT_RIGHT_OUT][i] = out[HT_RIGHT_CHAN];
  }
}

void CHilbertTransformPlugin::Deactivate()
{
}

