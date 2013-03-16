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
#include <jack/jack.h>
#include <list>
#include <pcrecpp.h>
#include "util/JSON.h"
#include "util/incltinyxml.h"
#include "util/condition.h"

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
    }

    const char* TypeStr()
    {
      if (m_flags & JackPortIsOutput)
        return "output";
      else
        return "input";
    }

    bool operator<(CJackPort& rhs)
    {
      return (m_name < rhs.m_name);
    }

    bool operator==(CJackPort& rhs)
    {
      return (m_name == rhs.m_name && m_flags == rhs.m_flags);
    }

    bool operator!=(CJackPort& rhs)
    {
      return !(*this == rhs);
    }

    const std::string& Name()  { return m_name;  }
    int                Flags() { return m_flags; }

  private:
    std::string m_name;
    int         m_flags;
};

class CPortConnector
{
  public:
    CPortConnector(CBobDSP& bobdsp);
    ~CPortConnector();

    bool Connect();
    void Disconnect();

    void Process(bool& checkconnect, bool& checkdisconnect, bool& updateports);

    void Stop();

    void            LoadSettingsFromFile(const std::string& filename);
    bool            ConnectionsFromXML(TiXmlElement* root, bool strict);
    bool            ConnectionsFromJSON(const std::string& json);
    CJSONGenerator* ConnectionsToJSON();
    TiXmlElement*   ConnectionsToXML();
    CJSONGenerator* PortIndexToJSON();
    CJSONGenerator* PortsToJSON();
    CJSONGenerator* PortsToJSON(const std::string& postjson);

  private:
    std::vector<CPortConnection> m_connections;
    std::vector<CPortConnection> m_removed;
    std::list<CJackPort>         m_jackports;
    bool                         m_stop;
    unsigned int                 m_portindex;
    CCondition                   m_condition;
    int                          m_waitingthreads;

    CBobDSP&       m_bobdsp;
    jack_client_t* m_client;
    bool           m_connected;
    bool           m_wasconnected;

    bool ConnectInternal();
    void ProcessInternal(bool& checkconnect, bool& checkdisconnect, bool& updateports);

    bool ConnectPorts();
    bool DisconnectPorts();
    void MatchConnection(std::vector<CPortConnection>::iterator& it, std::vector< std::pair<std::string, std::string> >::iterator& con,
                         bool& outmatch, bool& inmatch, bool removed);

    void UpdatePorts();
};

#endif //PORTCONNECTOR_H
