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

#include "dpl2encoder.h"
#include "util/ssedefs.h"
#include <string.h>
#include <stdlib.h>
#include <algorithm>

using namespace BobDSPLadspa;

#define CE_COEF (0.707106781f)  // sqrt(1/2)
#define SH_COEF (0.816496581f)  // sqrt(2/3)
#define SL_COEF (0.577350269f)  // sqrt(1/3)

#define LIMITERTIME (0.05f)

CDPL2Encoder::CDPL2Encoder(unsigned long samplerate)
{
  memset(m_ports, 0, sizeof(m_ports));
  memset(m_delaybuf, 0, sizeof(m_delaybuf));
  m_delaybufpos = 0;

  m_limsamples = lroundf((float)samplerate * LIMITERTIME);
  m_limpos     = 0;
  m_limgain    = 1.0f;
  m_limgainmul = 0.0f;
}

CDPL2Encoder::~CDPL2Encoder()
{
}

void CDPL2Encoder::ConnectPort(unsigned long port, LADSPA_Data* datalocation)
{
  if (port < NUMPORTS)
    m_ports[port] = datalocation;
}

void CDPL2Encoder::Activate()
{
}

void OPTIMIZE CDPL2Encoder::Run(unsigned long samplecount)
{
  float fl; //front-left
  float fr; //front-right
  float ce; //center
  float rl; //rear-left
  float rr; //rear-right
  float lt; //left-total
  float rt; //right-total

  float limval;

  bool pulsecompat = !!lroundf(*m_ports[PULSECTL]);

  for (unsigned long i = 0; i < samplecount; i++)
  {
    //read input samples from the delay buffers for the front channels,
    //read input samples from the hilbert transformers for the surround channels
    fl = m_delaybuf[0][m_delaybufpos];
    fr = m_delaybuf[1][m_delaybufpos];
    ce = m_delaybuf[2][m_delaybufpos];
    rl = m_hilberttransform[0].Process(m_ports[RL_IN][i]);
    rr = m_hilberttransform[1].Process(m_ports[RR_IN][i]);

    //store the input samples for the front channels into the delay buffers
    m_delaybuf[0][m_delaybufpos] = m_ports[FL_IN][i];
    m_delaybuf[1][m_delaybufpos] = m_ports[FR_IN][i];
    m_delaybuf[2][m_delaybufpos] = m_ports[CE_IN][i];

    //generate the output channels by adding the input channels with their respective coefficients
    lt = fl + ce * CE_COEF + rl * -SH_COEF + rr * -SL_COEF;
    rt = fr + ce * CE_COEF + rr *  SH_COEF + rl *  SL_COEF;

    if (pulsecompat)
    {
      //when connecting this ladspa plugin in pulseaudio to a stereo sink, by default it mixes the
      //center channel and the surround channels to left and right, and decreases the volume to prevent clipping
      //by applying this mix that effect will be undone, so that the full volume is available
      fl = lt * 1.25f - rt * 0.25f;
      fr = rt * 1.25f - lt * 0.25f;
      ce = (lt + rt) * 0.5f;
      rl = lt;
      rr = rt;

      //because the output might clip, pass the samples through a limiter with 50 ms hold and release times
      limval = std::max(fabsf(fl * m_limgain), fabsf(fr * m_limgain));
      limval = std::max(limval, fabsf(ce * m_limgain));
      limval = std::max(limval, fabsf(rl * m_limgain));
      limval = std::max(limval, fabsf(rr * m_limgain));
      if (limval > 1.0f)
      {
        m_limgain    = m_limgain / limval;
        m_limpos     = m_limsamples * 2;
        m_limgainmul = powf(1.0f / m_limgain, 1.0f / (float)m_limsamples);
      }

      //write all output ports
      m_ports[LT_OUT][i]    = fl * m_limgain;
      m_ports[RT_OUT][i]    = fr * m_limgain;
      m_ports[LTRT2_OUT][i] = ce * m_limgain;
      m_ports[LTS_OUT][i]   = rl * m_limgain;
      m_ports[RTS_OUT][i]   = rr * m_limgain;
    }
    else
    {
      //because the output might clip, pass the samples through a limiter with 50 ms hold and release times
      limval = std::max(fabsf(lt * m_limgain), fabsf(rt * m_limgain));
      if (limval > 1.0f)
      {
        m_limgain    = m_limgain / limval;
        m_limpos     = m_limsamples * 2;
        m_limgainmul = powf(1.0f / m_limgain, 1.0f / (float)m_limsamples);
      }

      //write LT and RT, set the other ports to zero
      m_ports[LT_OUT][i]    = lt * m_limgain;
      m_ports[RT_OUT][i]    = rt * m_limgain;
      m_ports[LTRT2_OUT][i] = 0.0f;
      m_ports[LTS_OUT][i]   = 0.0f;
      m_ports[RTS_OUT][i]   = 0.0f;
    }

    m_delaybufpos++;
    if (m_delaybufpos >= DELAYSAMPLES)
      m_delaybufpos = 0;

    //decrease the limiter
    if (m_limpos > m_limsamples)
    {
      m_limpos--;
    }
    else if (m_limpos > 0)
    {
      m_limgain *= m_limgainmul;
      m_limpos--;
      if (m_limpos == 0)
        m_limgain = 1.0f;
    }
  }
}

void CDPL2Encoder::Deactivate()
{
}

