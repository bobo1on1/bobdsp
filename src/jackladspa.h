/*
 * bobdsp
 * Copyright (C) Bob 2013
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

#ifndef JACKLADSPA_H
#define JACKLADSPA_H

#include <vector>

#include "jackclient.h"
#include "ladspaplugin.h"
#include "ladspainstance.h"
#include "util/mutex.h"

class CJackLadspa : public CJackClient
{
  public:
    CJackLadspa(CLadspaPlugin* plugin, const std::string& name, int nrinstances,
                double* gain, controlmap controlinputs);
    ~CJackLadspa();

    void MarkDelete()   { m_delete = true;   }
    bool NeedsDelete()  { return m_delete;   }
    void MarkRestart()  { m_restart = true;  }
    void ClearRestart() { m_restart = false; }
    bool NeedsRestart() { return m_restart;  }

    int  NrInstances()                   { return m_nrinstances;        }
    void SetNrInstances(int nrinstances) { m_nrinstances = nrinstances; }

    CLadspaPlugin*     Plugin()           { return m_plugin;        }
    double             GetGain(int index) { return m_gain[index];   }
    void               UpdateGain(double gain, int index);
    int                Samplerate()       { return m_samplerate;    }
    void               GetControlInputs(controlmap& controlinputs);
    void               UpdateControls(controlmap& controlinputs);

  private:
    bool           m_delete;
    bool           m_restart;
    CLadspaPlugin* m_plugin;
    int            m_nrinstances;

    CMutex         m_mutex;
    controlvalue   m_gain[2]; //pregain, postgain
    controlvalue   m_runninggain[2]; //copied from m_gain in the jack thread
    controlmap     m_controlinputs;
    controlmap     m_newcontrolinputs;

    std::vector<CLadspaInstance*> m_instances;

    void PreConnect();
    bool PreActivate();
    void PostDeactivate();
    void TransferNewControlInputs(controlmap& controlinputs);
    int  PJackSamplerateCallback(jack_nframes_t nframes);
    int  PJackBufferSizeCallback(jack_nframes_t nframes);
    void PJackProcessCallback(jack_nframes_t nframes);
};

#endif //JACKLADSPA_H
