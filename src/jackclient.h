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
#include <jack/jack.h>

#include "clientmessage.h"

#define PORTEVENT_REGISTERED   0x01
#define PORTEVENT_DEREGISTERED 0x02
#define PORTEVENT_CONNECTED    0x04
#define PORTEVENT_DISCONNECTED 0x08
#define SAMPLERATE_CHANGED     0x10

class CJackClient : public CMessagePump
{
  public:
    CJackClient(const std::string& name, const std::string& logname,
                const std::string& threadname, const char* sender = "jack client");
    ~CJackClient();

    bool Connect();
    void Disconnect();
    bool IsConnected()  { return m_connected;}

    jack_status_t      ExitStatus() { return m_exitstatus; }
    const std::string& ExitReason() { return m_exitreason; }

    const std::string& Name()       { return m_name;          }
    const std::string& LogName()    { return m_logname;       }

  protected:
    bool           m_connected;
    bool           m_wasconnected;
    jack_client_t* m_client;
    std::string    m_name;
    std::string    m_logname;
    std::string    m_threadname;
    int            m_samplerate;
    int            m_buffersize;
    jack_status_t  m_exitstatus;
    std::string    m_exitreason;
    int            m_events;

    bool         ConnectInternal();
    void         CheckMessages();

    virtual void PreConnect() {};
    virtual bool PreActivate() { return true; };
    virtual void PostDeactivate() {};

    static  void SJackThreadInitCallback(void *arg);
    void         PJackThreadInitCallback();

    static  int  SJackProcessCallback(jack_nframes_t nframes, void *arg);
    virtual void PJackProcessCallback(jack_nframes_t nframes) = 0;

    static  void SJackInfoShutdownCallback(jack_status_t code, const char *reason, void *arg);
    virtual void PJackInfoShutdownCallback(jack_status_t code, const char *reason);

    static  void SJackPortRegistrationCallback(jack_port_id_t port, int reg, void *arg);
    void         PJackPortRegistrationCallback(jack_port_id_t port, int reg);

    static  void SJackPortConnectCallback(jack_port_id_t a, jack_port_id_t b, int connect, void *arg);
    void         PJackPortConnectCallback(jack_port_id_t a, jack_port_id_t b, int connect);

    static  int  SJackSamplerateCallback(jack_nframes_t nframes, void *arg);
    virtual int  PJackSamplerateCallback(jack_nframes_t nframes);

    static  int  SJackBufferSizeCallback(jack_nframes_t nframes, void *arg);
    virtual int  PJackBufferSizeCallback(jack_nframes_t nframes);
};

#endif //JACKCLIENT_H
