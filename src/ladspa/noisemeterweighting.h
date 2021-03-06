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

#ifndef NOISEMETERWEIGHTING_H
#define NOISEMETERWEIGHTING_H

#include "util/inclstdint.h"
#include "filterdescriptions.h"
#include "filterinterface.h"
#include "noisemeter/acfilter.h"
#include "noisemeter/itu468filter.h"
#include "noisemeter/lpeq20filter.h"

namespace BobDSPLadspa
{
  class CNoiseMeterWeighting : public IFilter
  {
    public:
      CNoiseMeterWeighting(unsigned long samplerate);
      ~CNoiseMeterWeighting();

      void ConnectPort(unsigned long port, LADSPA_Data* datalocation);
      void Activate();
      void Run(unsigned long samplecount);
      void Deactivate();

    private:
      void          InitFilter();

      LADSPA_Data*  m_ports[3];
      int32_t       m_type;
      unsigned long m_samplerate;

      ACfilter      m_acfilter;
      Itu468filter  m_itu468filter;
      LPeq20filter  m_lpeq20filter;
  };
}

#endif //NOISEMETERWEIGHTING_H
