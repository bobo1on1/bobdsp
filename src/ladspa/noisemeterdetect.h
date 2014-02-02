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

#ifndef NOISEMETERDETECT_H
#define NOISEMETERDETECT_H

#include "util/inclstdint.h"
#include "filterdescriptions.h"
#include "filterinterface.h"
#include "noisemeter/itu468detect.h"
#include "noisemeter/rmsdetect.h"
#include "noisemeter/vumdetect.h"

namespace BobDSPLadspa
{
  class CNoiseMeterDetect : public IFilter
  {
    public:
      CNoiseMeterDetect(unsigned long samplerate);
      ~CNoiseMeterDetect();

      void ConnectPort(unsigned long port, LADSPA_Data* datalocation);
      void Activate();
      void Run(unsigned long samplecount);
      void Deactivate();

    private:
      void          InitFilter();

      LADSPA_Data*  m_ports[4];
      int32_t       m_type;
      unsigned long m_samplerate;
      int32_t       m_slow;

      Itu468detect  m_itu468detect;
      RMSdetect     m_rmsdetect;
      VUMdetect     m_vumdetect;
  };
}

#endif //NOISEMETERDETECT_H
