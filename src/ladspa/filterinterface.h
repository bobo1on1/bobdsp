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

#ifndef BIQUADINTERFACE_H
#define BIQUADINTERFACE_H

namespace BobDSPLadspa
{
  LADSPA_Handle Instantiate(const struct _LADSPA_Descriptor* Descriptor, unsigned long samplerate);
  void ConnectPort(LADSPA_Handle instance, unsigned long port, LADSPA_Data* datalocation);
  void Activate(LADSPA_Handle instance);
  void Run(LADSPA_Handle instance, unsigned long samplecount);
  void Deactivate(LADSPA_Handle instance);
  void Cleanup(LADSPA_Handle instance);

  class IFilter
  {
    public:
      virtual ~IFilter() {};
      virtual void ConnectPort(unsigned long port, LADSPA_Data* datalocation) = 0;
      virtual void Activate() = 0;
      virtual void Run(unsigned long samplecount) = 0;
      virtual void Deactivate() = 0;
  };
}

#endif //BIQUADINTERFACE_H
