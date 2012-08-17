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

enum ClientMessage
{
  MsgNone,
  MsgExited,
  MsgPortRegistered,
  MsgPortConnected,
};

class CJackClient
{
  public:
    CJackClient(CLadspaPlugin* plugin, const std::string& name, int nrinstances,
                float pregain, float postgain, std::vector<portvalue> controlinputs,
                const std::string& clientprefix, const std::string& portprefix);
    ~CJackClient();

    bool Connect();
    void Disconnect();
    bool IsConnected() { return m_connected; }
    int  MsgPipe()     { return m_pipe[0];   }
    ClientMessage GetMessage();

    const std::string& Name() { return m_name; }

    jack_status_t      ExitStatus() { return m_exitstatus; }
    const std::string& ExitReason() { return m_exitreason; }

  private:
    bool           m_connected;
    bool           m_wasconnected;
    jack_client_t* m_client;
    CLadspaPlugin* m_plugin;
    std::string    m_name;
    std::string    m_clientprefix;
    std::string    m_portprefix;
    int            m_nrinstances;
    float          m_pregain;
    float          m_postgain;
    int            m_samplerate;
    jack_status_t  m_exitstatus;
    std::string    m_exitreason;
    bool           m_portregistered;
    bool           m_portconnected;
    int            m_pipe[2];

    std::vector<CLadspaInstance*> m_instances;
    std::vector<portvalue>        m_controlinputs;

    bool        ConnectInternal();
    bool        ConnectJackPorts();
    void        InitLadspa();
    void        CheckMessages();
    bool        WriteMessage(uint8_t message);

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
