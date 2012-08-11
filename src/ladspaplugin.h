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

#ifndef LADSPAPLUGIN_H
#define LADSPAPLUGIN_H

#include <vector>
#include <string>
#include <ladspa.h>

class CLadspaPlugin
{
  public:
    CLadspaPlugin(const std::string& filename, void* handle, const LADSPA_Descriptor* descriptor);
    ~CLadspaPlugin();

    static void GetPlugins(std::string path, std::vector<CLadspaPlugin*>& plugins);

    void LoadAllSymbols();

    const LADSPA_Descriptor* Descriptor() { return m_descriptor; }
    std::string&             FileName()   { return m_filename;   }

    unsigned long UniqueID()              { return m_descriptor->UniqueID;  }
    const char*   Label()                 { return m_descriptor->Label;     }
    unsigned long PortCount()             { return m_descriptor->PortCount; }

    const LADSPA_PortDescriptor  PortDescriptor(unsigned long port);
    const char*                  PortName(unsigned long port);

    int AudioInputPorts();
    int AudioOutputPorts();

  private:
    const LADSPA_Descriptor* m_descriptor;
    std::string              m_filename;
    void*                    m_handle;
};

#endif //LADSPAPLUGIN_H
