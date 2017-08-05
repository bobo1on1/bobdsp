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

#ifndef DPL2ENCODER_H
#define DPL2ENCODER_H

#include "filterdescriptions.h"
#include "filterinterface.h"

#include "hilberttransform.h"

#include <math.h>

#define NUMPORTS 11

//this channel map is compatible with the default pulseaudio channel map
#define FL_IN     0
#define FR_IN     1
#define CE_IN     2
#define RL_IN     3
#define RR_IN     4
#define LT_OUT    5
#define RT_OUT    6
#define LTRT2_OUT 7
#define LTS_OUT   8
#define RTS_OUT   9
#define PULSECTL 10

#define DELAYSAMPLES  (FILTERSIZE / 2)
#define DELAYCHANNELS (3)

namespace BobDSPLadspa
{
  class CDPL2Encoder : public IFilter
  {
    public:
      CDPL2Encoder(unsigned long samplerate);
      ~CDPL2Encoder();

      void ConnectPort(unsigned long port, LADSPA_Data* datalocation);
      void Activate();
      void Run(unsigned long samplecount);
      void Deactivate();

    private:
      void Reset();

      LADSPA_Data*      m_ports[NUMPORTS];

      float             m_delaybuf[DELAYCHANNELS][DELAYSAMPLES];
      int               m_delaybufpos;
      CHilbertTransform m_hilberttransform[2];

      int               m_limsamples;
      float             m_limgain;
      int               m_limpos;
      float             m_limgainmul;
  };
}

#endif //DPL2ENCODER_H
