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

#ifndef BIQUADCOEFS_H
#define BIQUADCOEFS_H

#include <ladspa.h>
#include "filterdescriptions.h"

namespace BobDSPLadspa
{
  class CBiquadCoef
  {
    public:
      CBiquadCoef();
      ~CBiquadCoef();

      void Calculate(EFILTER type, float samplerate, LADSPA_Data** ports);

      LADSPA_Data a0;
      LADSPA_Data a1;
      LADSPA_Data a2;
      LADSPA_Data b0;
      LADSPA_Data b1;
      LADSPA_Data b2;

    private:
      void Passthrough();
      void LinkwitzTransform(float samplerate, LADSPA_Data** ports);

      bool        m_initialized;
      LADSPA_Data m_oldsettings[4];
  };
}

#endif //BIQUADCOEFS_H
