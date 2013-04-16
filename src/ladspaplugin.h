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

#include <string>
#include <ladspa.h>

class CLadspaPlugin
{
  public:
    CLadspaPlugin(const std::string& filename, void* handle, const LADSPA_Descriptor* descriptor);
    ~CLadspaPlugin();

    static bool Sort(CLadspaPlugin* first, CLadspaPlugin* second);

    void LoadAllSymbols();

    const LADSPA_Descriptor* Descriptor() { return m_descriptor; }
    const std::string&       FileName()   { return m_filename;   }

    unsigned long UniqueID()              { return m_descriptor->UniqueID;  }
    const char*   Label()                 { return m_descriptor->Label;     }
    const char*   Name()                  { return m_descriptor->Name;      }
    const char*   Maker()                 { return m_descriptor->Maker;     }
    const char*   Copyright()             { return m_descriptor->Copyright; }
    unsigned long PortCount()             { return m_descriptor->PortCount; }

    const LADSPA_PortDescriptor PortDescriptor(unsigned long port);
    bool                        PortDescriptorSanityCheck(unsigned long port);
    const LADSPA_PortRangeHint  PortRangeHint(unsigned long port);
    const char*                 PortName(unsigned long port);
    long                        PortByName(const std::string& portname);
    const char*                 DirectionStr(unsigned long port);
    const char*                 TypeStr(unsigned long port);
    bool                        IsControl(unsigned long port);
    bool                        IsAudio(unsigned long port);
    bool                        IsInput(unsigned long port);
    bool                        IsOutput(unsigned long port);
    bool                        IsControlInput(unsigned long port);
    bool                        IsControlOutput(unsigned long port);
    bool                        IsAudioInput(unsigned long port);
    bool                        IsAudioOutput(unsigned long port);
    bool                        HasLowerBound(unsigned long port);
    bool                        HasUpperBound(unsigned long port);
    float                       LowerBound(unsigned long port, int samplerate);
    float                       UpperBound(unsigned long port, int samplerate);
    bool                        IsToggled(unsigned long port);
    bool                        IsLogarithmic(unsigned long port);
    bool                        IsInteger(unsigned long port);
    bool                        HasDefault(unsigned long port);
    float                       DefaultValue(unsigned long port, int samplerate);

    int AudioInputPorts();
    int AudioOutputPorts();
    int ControlInputPorts();
    int ControlOutputPorts();

  private:
    float MakeDefault(bool islog, float low, float high, float interpolate);

    const LADSPA_Descriptor* m_descriptor;
    std::string              m_filename;
    void*                    m_handle;
    bool                     m_fullyloaded;
};

#endif //LADSPAPLUGIN_H
