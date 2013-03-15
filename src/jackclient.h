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

#ifndef JACKCLIENT_H
#define JACKCLIENT_H

#include <string>
#include <vector>
#include <jack/jack.h>

#include "ladspaplugin.h"
#include "ladspainstance.h"
#include "clientmessage.h"
#include "util/mutex.h"

class CJackClient : public CMessagePump
{
  public:
    CJackClient(CLadspaPlugin* plugin, const std::string& name, int nrinstances,
                double* gain, controlmap controlinputs);
    ~CJackClient();

    bool Connect();
    void Disconnect();
    bool IsConnected()  { return m_connected;}
    void MarkDelete()   { m_delete = true;   }
    bool NeedsDelete()  { return m_delete;   }
    void MarkRestart()  { m_restart = true;  }
    bool NeedsRestart() { return m_restart;  }

    int  NrInstances()                   { return m_nrinstances;        }
    void SetNrInstances(int nrinstances) { m_nrinstances = nrinstances; }

    jack_status_t      ExitStatus() { return m_exitstatus; }
    const std::string& ExitReason() { return m_exitreason; }

    CLadspaPlugin*                Plugin()           { return m_plugin;        }
    const std::string&            Name()             { return m_name;          }
    float*                        GetGain()          { return m_gain;          }
    void                          UpdateGain(float gain, int index);
    int                           Samplerate()       { return m_samplerate;    }
    void                          GetControlInputs(controlmap& controlinputs);
    void                          UpdateControls(controlmap& controlinputs);

  private:
    bool           m_connected;
    bool           m_wasconnected;
    bool           m_delete;
    bool           m_restart;
    jack_client_t* m_client;
    CLadspaPlugin* m_plugin;
    std::string    m_name;
    int            m_nrinstances;
    int            m_samplerate;
    jack_status_t  m_exitstatus;
    std::string    m_exitreason;
    int            m_portevents;
    bool           m_nameset;

    CMutex         m_mutex;
    float          m_gain[2]; //pregain, postgain
    float          m_runninggain[2]; //copied from m_gain in the jack thread
    controlmap     m_controlinputs;
    controlmap     m_newcontrolinputs;

    std::vector<CLadspaInstance*> m_instances;

    bool        ConnectInternal();
    bool        ConnectJackPorts();
    void        InitLadspa();
    void        CheckMessages();
    void        TransferNewControlInputs(controlmap& controlinputs);

    static int  SJackProcessCallback(jack_nframes_t nframes, void *arg);
    void        PJackProcessCallback(jack_nframes_t nframes);

    static void SJackInfoShutdownCallback(jack_status_t code, const char *reason, void *arg);
    void        PJackInfoShutdownCallback(jack_status_t code, const char *reason);

    static void SJackPortRegistrationCallback(jack_port_id_t port, int reg, void *arg);
    void        PJackPortRegistrationCallback(jack_port_id_t port, int reg);

    static void SJackPortConnectCallback(jack_port_id_t a, jack_port_id_t b, int connect, void *arg);
    void        PJackPortConnectCallback(jack_port_id_t a, jack_port_id_t b, int connect);
};

#endif //JACKCLIENT_H
