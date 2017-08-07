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

#ifndef HILBERTTRANSFORMPLUGIN_H
#define HILBERTTRANSFORMPLUGIN_H

#include "filterdescriptions.h"
#include "filterinterface.h"

#include "hilberttransform.h"

#define HT_LEFT_IN      0
#define HT_RIGHT_IN     1
#define HT_LEFT_OUT     2
#define HT_RIGHT_OUT    3
#define HT_LEFT_DRYWET  4
#define HT_RIGHT_DRYWET 5

#define HT_NUMPORTS     6
#define HT_NUMCHANNELS  2

#define HT_LEFT_CHAN    0
#define HT_RIGHT_CHAN   1

namespace BobDSPLadspa
{
  class CHilbertTransformPlugin : public IFilter
  {
    public:
      CHilbertTransformPlugin();
      ~CHilbertTransformPlugin();

      void ConnectPort(unsigned long port, LADSPA_Data* datalocation);
      void Activate();
      void Run(unsigned long samplecount);
      void Deactivate();

    private:
      LADSPA_Data*      m_ports[HT_NUMPORTS];
      CHilbertTransform m_hilberttransform[HT_NUMCHANNELS];
      LADSPA_Data       m_delaybuf[HT_NUMCHANNELS][FILTERSIZE / 2];
      int               m_delaybufpos;
  };
}
#endif //HILBERTTRANSFORMPLUGIN_H
