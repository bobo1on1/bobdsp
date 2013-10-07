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

#ifndef FILTERDESCRIPTIONS_H
#define FILTERDESCRIPTIONS_H

#include <ladspa.h>

//bobdsp has ladspa plugin id's 4901 to 4940 reserved

enum EFILTER
{
  LINKWITZTRANSFORM = 4901,
  DITHER,
  ECHOCANCELLATION
};

namespace BobDSPLadspa
{
  class CFilterDescriptions
  {
    public:
      static const LADSPA_Descriptor* Descriptor(unsigned long index);
      static unsigned long NrDescriptors();

    private:
      static const LADSPA_Descriptor m_descriptors[];
  };
}

#endif //FILTERDESCRIPTIONS_H
