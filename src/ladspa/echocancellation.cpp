/*
 * bobdsp
 * Copyright (C) Bob 2013
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
#include "echocancellation.h"
#include "util/misc.h"
#include "util/ssedefs.h"
#include "util/inclstdint.h"

using namespace BobDSPLadspa;

CEchoCancellation::CEchoCancellation(unsigned long samplerate)
{
  memset(m_ports, 0, sizeof(m_ports));
  m_echostate = NULL;
  m_samplerate = samplerate;
  m_filterlength = -1.0f;
  m_framesize = 0;
}

CEchoCancellation::~CEchoCancellation()
{
  if (m_echostate)
    speex_echo_state_destroy(m_echostate);
}

void CEchoCancellation::ConnectPort(unsigned long port, LADSPA_Data* datalocation)
{
  m_ports[port] = datalocation;
}

void CEchoCancellation::Activate()
{
}

inline void OPTIMIZE FloatToInt16(float* in, int16_t* out, unsigned long samplecount)
{
  float*   inptr  = in;
  float*   inend  = in + samplecount;
  int16_t* outptr = out;

  while (inptr != inend)
    *(outptr++) = Clamp(Round32(*(inptr++) * 32768.0f), INT16_MIN, INT16_MAX);
}

inline void OPTIMIZE Int16ToFloat(int16_t* in, float* out, unsigned long samplecount)
{
  int16_t* inptr  = in;
  int16_t* inend  = in + samplecount;
  float*   outptr = out;

  while (inptr != inend)
    *(outptr++) = (float)*(inptr++) / 32768.0f;
}

void CEchoCancellation::Run(unsigned long samplecount)
{
  if (m_echostate == NULL || *(m_ports[3]) != m_filterlength || samplecount != m_framesize)
  {
    m_filterlength = *(m_ports[3]);
    m_framesize = samplecount;
    if (m_echostate)
      speex_echo_state_destroy(m_echostate);

    m_echostate = speex_echo_state_init(m_framesize, Max(Round32(m_filterlength * (float)m_samplerate), 1));
  }

  int16_t inbuf[samplecount];
  int16_t echobuf[samplecount];
  int16_t outbuf[samplecount];

  FloatToInt16(m_ports[0], inbuf, samplecount);
  FloatToInt16(m_ports[1], echobuf, samplecount);

  speex_echo_cancellation(m_echostate, inbuf, echobuf, outbuf);

  Int16ToFloat(outbuf, m_ports[2], samplecount);
}

void CEchoCancellation::Deactivate()
{
}

