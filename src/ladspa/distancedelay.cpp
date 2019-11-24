/*
 * bobdsp
 * Copyright (C) Bob 2019
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

#include "distancedelay.h"
#include "util/misc.h"

using namespace BobDSPLadspa;

#define SPEED_OF_SOUND 343.0 //speed of sound in m/s
#define MAX_DISTANCE   100.0 //max distance delay that can be added

CDistanceDelay::CDistanceDelay(unsigned long samplerate) : m_ports {}
{
  m_samplerate   = samplerate;
  m_delaybufsize = Round32(MAX_DISTANCE / SPEED_OF_SOUND * samplerate);
  m_delaybufpos  = 0;

  //allocate buffers for the maximum delay
  for (int i = 0; i < DISTANCECHANNELS; i++)
  {
    m_delaybuf[i] = new LADSPA_Data[m_delaybufsize];
    memset(m_delaybuf[i], 0, m_delaybufsize * sizeof(LADSPA_Data));
  }
}

CDistanceDelay::~CDistanceDelay()
{
  for (int i = 0; i < DISTANCECHANNELS; i++)
    delete[] m_delaybuf[i];
}

void CDistanceDelay::ConnectPort(unsigned long port, LADSPA_Data* datalocation)
{
  m_ports[port] = datalocation;
}

void CDistanceDelay::Activate()
{
}

void CDistanceDelay::Run(unsigned long samplecount)
{
  //the settings ports set the delay in meters, calculate how many samples
  //to delay based on the speed of sound and the sample rate
  int delays[DISTANCECHANNELS] = {};
  for (int c = 0; c < DISTANCECHANNELS; c++)
  {
    delays[c] = Round32(Clamp(*m_ports[c + DISTANCECHANNELS * 2], 0.0, MAX_DISTANCE) /
                        SPEED_OF_SOUND * m_samplerate);
  }

  //calculate where in the delay buffer to write and read samples
  int writepos = m_delaybufpos;
  int readpos[DISTANCECHANNELS] = {};
  for (int c = 0; c < DISTANCECHANNELS; c++)
  {
    readpos[c] = m_delaybufpos - delays[c];
    if (readpos[c] < 0)
      readpos[c] += m_delaybufsize;
  }

  //write samples to the delay buffer, and read them back at a delayed position
  for (unsigned long i = 0; i < samplecount; i++)
  {
    for (int c = 0; c < DISTANCECHANNELS; c++)
    {
      m_delaybuf[c][writepos] = m_ports[c][i];
      m_ports[c + DISTANCECHANNELS][i] = m_delaybuf[c][readpos[c]];

      readpos[c]++;
      if (readpos[c] >= m_delaybufsize)
        readpos[c] = 0;
    }

    writepos++;
    if (writepos >= m_delaybufsize)
      writepos = 0;
  }

  m_delaybufpos = writepos;
}

void CDistanceDelay::Deactivate()
{
}
