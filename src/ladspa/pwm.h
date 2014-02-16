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

#ifndef PWM_H
#define PWM_H

#include "util/inclstdint.h"
#include "filterdescriptions.h"
#include "filterinterface.h"

namespace BobDSPLadspa
{
  class CPwm : public IFilter
  {
    public:
      CPwm(unsigned long samplerate);
      ~CPwm();

      void ConnectPort(unsigned long port, LADSPA_Data* datalocation);
      void Activate();
      void Run(unsigned long samplecount);
      void Deactivate();

    private:
      unsigned long m_samplerate;
      LADSPA_Data*  m_ports[3];
      float         m_state;
      uint32_t      m_samplecounter;
      double        m_accumulator;
      uint32_t      m_accumsamples;
      uint32_t      m_outval;
  };
}

#endif //PWM_H
