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

#ifndef SWITCH_H
#define SWITCH_H

#include "util/inclstdint.h"
#include "filterdescriptions.h"
#include "filterinterface.h"

namespace BobDSPLadspa
{
  class CSwitch : public IFilter
  {
    public:
      CSwitch(unsigned long samplerate);
      ~CSwitch();

      void ConnectPort(unsigned long port, LADSPA_Data* datalocation);
      void Activate();
      void Run(unsigned long samplecount);
      void Deactivate();

    private:
      void Setup();

      unsigned long m_samplerate;
      LADSPA_Data*  m_ports[6];
      LADSPA_Data   m_prevports[3];
      uint32_t      m_samplecounter;
      float         m_level;
      uint32_t      m_turnondelay;
      uint32_t      m_turnoffdelay;
      float         m_state;
  };
}

#endif //SWITCH_H
