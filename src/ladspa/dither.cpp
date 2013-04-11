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
#include <stdlib.h>
#include <sys/time.h>
#include "dither.h"
#include "util/misc.h"
#include "util/inclstdint.h"
#include "util/ssedefs.h"

using namespace BobDSPLadspa;

CDither::CDither()
{
  memset(m_ports, 0, sizeof(m_ports));

  //use seconds and microseconds of gettimeofday as seed for rand_r
  struct timeval tv;
  gettimeofday(&tv, NULL);
  m_rand = tv.tv_sec + tv.tv_usec;
}

CDither::~CDither()
{
}

void CDither::ConnectPort(unsigned long port, LADSPA_Data* datalocation)
{
  m_ports[port] = datalocation;
}

void CDither::Activate()
{
}

void OPTIMIZE CDither::Run(unsigned long samplecount)
{
  LADSPA_Data* in    = m_ports[0];
  LADSPA_Data* inend = m_ports[0] + samplecount;
  LADSPA_Data* out   = m_ports[1];

  LADSPA_Data precision = powf(2.0f, Round32(Clamp(*(m_ports[2]), 1.0f, 32.0f)) - 1.0f);

  //add one bit noise twice to each sample, this will add noise with triangular distribution
  //when the samples are converted to integer of the set bitdepth for playback on the soundcard,
  //the noise will dither the audio
  while (in != inend)
    *(out++) = *(in++) + ((LADSPA_Data)rand_r(&m_rand) / RAND_MAX +
               (LADSPA_Data)rand_r(&m_rand) / RAND_MAX - 1.0f) / precision;
}

void CDither::Deactivate()
{
}

