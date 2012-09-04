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
#include "util/JSON.h"
#include "util/incltinyxml.h"
#include "util/mutex.h"

class CBobDSP;

class portconnection
{
  public:
    std::string in;
    std::string out;
    bool indisconnect;
    bool outdisconnect;

    const char* DisconnectStr()
    {
      if (outdisconnect && indisconnect)
        return "both";
      else if (outdisconnect)
        return "out";
      else if (indisconnect)
        return "in";
      else
        return "none";
    }

    bool operator==(portconnection& rhs)
    {
      return (out == rhs.out && in == rhs.in);
    }
};

class CJackPort
{
  public:
    CJackPort(std::string a_name, int a_flags)
    {
      name = a_name;
      flags = a_flags;
    }

    std::string name;
    int         flags;
};

class CPortConnector
{
  public:
    CPortConnector(CBobDSP& bobdsp);
    ~CPortConnector();

    bool Connect();
    void Disconnect();

    void Process(bool& checkconnect, bool& checkdisconnect, bool& updateports);

    bool          ConnectionsFromXML(TiXmlElement* root, bool strict);
    bool          ConnectionsFromJSON(const std::string& json);
    std::string   ConnectionsToJSON();
    TiXmlElement* ConnectionsToXML();

  private:
    std::vector<portconnection> m_connections;
    std::vector<portconnection> m_removed;
    std::vector<CJackPort>      m_jackports;
    CMutex                      m_mutex;

    CBobDSP&       m_bobdsp;
    jack_client_t* m_client;
    bool           m_connected;

    bool ConnectInternal();
    void ProcessInternal(bool& checkconnect, bool& checkdisconnect, bool& updateports);

    void ConnectPorts(const char** ports);
    void DisconnectPorts(const char** ports);
    void MatchConnection(std::vector<portconnection>::iterator& it, std::vector< std::pair<std::string, std::string> >::iterator& con,
                         bool& outmatch, bool& inmatch, bool removed);

    void UpdatePorts(const char** ports);
};

#endif //PORTCONNECTOR_H
