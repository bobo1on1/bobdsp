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

#ifndef PORTCONNECTOR_H
#define PORTCONNECTOR_H

#include <string>
#include <vector>
#include <list>
#include <utility>
#include <pcrecpp.h>
#include "util/JSON.h"
#include "util/condition.h"
#include "util/alphanum.h"
#include "util/misc.h"
#include "jsonsettings.h"
#include "jackclient.h"

class CBobDSP;

class CPortConnection
{
  public:
    CPortConnection(const std::string& out, const std::string& in, bool outdisconnect, bool indisconnect):
      m_outre(out),
      m_inre(in)
    {
      m_out = out;
      m_in = in;
      m_outdisconnect = outdisconnect;
      m_indisconnect = indisconnect;
    }

    const char* DisconnectStr()
    {
      if (m_outdisconnect && m_indisconnect)
        return "both";
      else if (m_outdisconnect)
        return "out";
      else if (m_indisconnect)
        return "in";
      else
        return "none";
    }

    bool operator==(CPortConnection& rhs)
    {
      return (m_out == rhs.m_out && m_in == rhs.m_in);
    }

    const std::string& Out() { return m_out; }
    const std::string& In()  { return m_in;  }

    bool OutDisconnect() { return m_outdisconnect; }
    bool InDisconnect()  { return m_indisconnect;  }

    bool OutMatch(const std::string& out) { return m_outre.FullMatch(out); }
    bool InMatch(const std::string& in)   { return m_inre.FullMatch(in);   }

  private:
    std::string m_out;
    std::string m_in;
    bool m_outdisconnect;
    bool m_indisconnect;

    pcrecpp::RE m_outre;
    pcrecpp::RE m_inre;
};

class CJackPort
{
  public:
    CJackPort(std::string name, int flags)
    {
      m_name = name;
      m_flags = flags;

      //store the port name as lower case too, to do a case insensitive sort
      ToLower(m_name, m_lname);
    }

    const char* DirectionStr()
    {
      if (IsOutput())
        return "output";
      else if (IsInput())
        return "input";
      else
        return "none";
    }

    bool IsOutput()
    {
      return (m_flags & JackPortIsOutput) == JackPortIsOutput;
    }

    bool IsInput()
    {
      return (m_flags & JackPortIsInput) == JackPortIsInput;
    }

    bool operator<(CJackPort& rhs)
    {
      return alphanum_comp(m_lname.c_str(), rhs.m_lname.c_str()) < 0;
    }

    bool operator==(const CJackPort& rhs)
    {
      return m_name == rhs.m_name &&
             ((m_flags & JackPortIsOutput) == (rhs.m_flags & JackPortIsOutput) ||
              (m_flags & JackPortIsInput) == (rhs.m_flags & JackPortIsInput));
    }

    bool operator!=(const CJackPort& rhs)
    {
      return !(*this == rhs);
    }

    const std::string& Name()  { return m_name;  }
    int                Flags() { return m_flags; }

  private:
    std::string m_name;
    std::string m_lname;
    int         m_flags;
};

class CPortConnector : public CJackClient, public CJSONSettings
{
  public:
    CPortConnector(CBobDSP& bobdsp);
    ~CPortConnector();

    bool Process();
    void Stop();

    CJSONGenerator* ConnectionsToJSON();
    CJSONGenerator* PortIndexToJSON();
    CJSONGenerator* PortsToJSON();
    CJSONGenerator* PortsToJSON(const std::string& postjson, const std::string& source);

  private:
    std::vector<CPortConnection> m_connections;
    std::vector<CPortConnection> m_removed;
    std::list<CJackPort>         m_jackports;
    bool                         m_stop;
    unsigned int                 m_portindex;
    CCondition                   m_condition;
    int                          m_waitingthreads;
    bool                         m_connectionsupdated;
    bool                         m_checkupdate;
    bool                         m_checkconnect;
    bool                         m_checkdisconnect;
    CBobDSP&                     m_bobdsp;

    std::vector< std::pair<int, CJackPort> > m_portchanges;
    CMutex                                   m_portchangelock;

    virtual CJSONGenerator* SettingsToJSON(bool tofile);
    virtual void            LoadSettings(JSONMap& root, bool reload, bool allowreload, const std::string& source);

    void LoadConnections(JSONArray& jsonconnections, const std::string& source);

    void ProcessMessage(ClientMessage msg);
    bool ConnectPorts();
    bool DisconnectPorts();
    void MatchConnection(std::vector<CPortConnection>::iterator& it, std::vector< std::pair<std::string,
                         std::string> >::iterator& con, bool& outmatch, bool& inmatch, bool removed);

    void UpdatePorts();
    void ProcessUpdates();

    bool PreActivate();
    void PJackPortRegistrationCallback(jack_port_id_t port, int reg);
    void PJackPortConnectCallback(jack_port_id_t a, jack_port_id_t b, int connect);
};

#endif //PORTCONNECTOR_H
