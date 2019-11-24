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

#ifndef DISTANCEDELAY_H
#define DISTANCEDELAY_H

#include "filterdescriptions.h"
#include "filterinterface.h"

#define DISTANCECHANNELS 2

namespace BobDSPLadspa
{
  class CDistanceDelay : public IFilter
  {
    public:
      CDistanceDelay(unsigned long samplerate);
      ~CDistanceDelay();

      void ConnectPort(unsigned long port, LADSPA_Data* datalocation);
      void Activate();
      void Run(unsigned long samplecount);
      void Deactivate();

    private:
      LADSPA_Data  m_samplerate;
      LADSPA_Data* m_ports[6];
      LADSPA_Data* m_delaybuf[DISTANCECHANNELS];
      int          m_delaybufsize;
      int          m_delaybufpos;
  };
}
#endif //DISTANCEDELAY_H
