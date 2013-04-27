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

#include "util/misc.h"
#include <utility>
#include <errno.h>
#include <memory>
#include <fstream>

#include "bobdsp.h"
#include "portconnector.h"
#include "util/log.h"
#include "util/lock.h"

using namespace std;

#define SETTINGSFILE ".bobdsp/connections.json"

CPortConnector::CPortConnector(CBobDSP& bobdsp) :
  CMessagePump("portconnector"),
  CJSONSettings(SETTINGSFILE, "connection", m_condition),
  m_bobdsp(bobdsp)
{
  m_client = NULL;
  m_connected = false;
  m_wasconnected = true;
  m_stop = false;
  m_portindex = 0;
  m_waitingthreads = 0;
  m_connectionsupdated = false;
}

CPortConnector::~CPortConnector()
{
  Disconnect();
}

bool CPortConnector::Connect()
{
  m_connected = ConnectInternal();
  if (!m_connected)
    Disconnect();
  else
    m_wasconnected = true;

  return m_connected;
}

void CPortConnector::Disconnect()
{
  if (m_client)
  {
    LogDebug("Disconnecting portconnector");

    int returnv = jack_client_close(m_client);
    if (returnv != 0)
      LogError("Portconnector error %i closing client: \"%s\"",
               returnv, GetErrno().c_str());

    m_client = NULL;
  }

  m_connected = false;
}

CJSONGenerator* CPortConnector::SettingsToJSON(bool tofile)
{
  return ConnectionsToJSON();
}

void CPortConnector::LoadSettings(JSONMap& root, bool reload, bool allowreload, const std::string& source)
{
  //check the connections array and action string first, then parse them if they're valid
  JSONMap::iterator connections = root.find("connections");
  if (connections != root.end() && !connections->second->IsArray())
  {
    LogError("%s: invalid value for connections: %s", source.c_str(), ToJSON(connections->second).c_str());
    return;
  }

  JSONMap::iterator action = root.find("action");
  if (action != root.end() && !action->second->IsString())
  {
    LogError("%s: invalid value for action: %s", source.c_str(), ToJSON(action->second).c_str());
    return;
  }

  if (connections != root.end())
  {
    LoadConnections(connections->second->AsArray(), source);
    m_connectionsupdated = true;
  }

  if (action != root.end())
  {
    if (action->second->AsString() == "save")
    {
      SaveFile();
    }
    else if (action->second->AsString() == "reload" && allowreload)
    {
      m_connectionsupdated = true;
      LoadFile(true);
    }
  }

  //if the connections are updated, signal the main thread
  if (m_connectionsupdated)
    m_connectionsupdated = !WriteMessage(MsgConnectionsUpdated);
}

void CPortConnector::LoadConnections(JSONArray& jsonconnections, const std::string& source)
{
  std::vector<CPortConnection> connections;

  //iterate over the connections, make sure they're valid, then store them in conarray
  for (JSONArray::iterator it = jsonconnections.begin(); it != jsonconnections.end(); it++)
  {
    if (!(*it)->IsMap())
    {
      LogError("%s: invalid value for connection %s", source.c_str(), ToJSON(*it).c_str());
      continue;
    }

    bool valid = false;
    JSONMap::iterator regex[2];
    JSONMap::iterator disconnect[2];
    for (int i = 0; i < 2; i++)
    {
      string search(i == 0 ? "out" : "in");

      regex[i] = (*it)->AsMap().find(search);
      if (regex[i] == (*it)->AsMap().end())
      {
        LogError("%s: %s value missing", source.c_str(), search.c_str());
        break;
      }
      else if (!regex[i]->second->IsString())
      {
        LogError("%s: invalid value for %s: %s", source.c_str(), search.c_str(), ToJSON(regex[i]->second).c_str());
        break;
      }

      search += "disconnect";
      disconnect[i] = (*it)->AsMap().find(search);
      if (disconnect[i] == (*it)->AsMap().end())
      {
        LogError("%s: %s value missing", source.c_str(), search.c_str());
        break;
      }
      else if (!disconnect[i]->second->IsBool())
      {
        LogError("%s: invalid value for %s: %s", source.c_str(), search.c_str(), ToJSON(disconnect[i]->second).c_str());
        break;
      }

      if (i == 1)
        valid = true;
    }

    if (valid)
    {
      connections.push_back(CPortConnection(regex[0]->second->AsString(), regex[1]->second->AsString(),
                                         disconnect[0]->second->AsBool(), disconnect[1]->second->AsBool()));

      LogDebug("Loaded connection out:\"%s\" in:\"%s\", outdisconnect:%s, indisconnect:%s",
               connections.back().Out().c_str(), connections.back().In().c_str(),
               ToString(connections.back().OutDisconnect()).c_str(), ToString(connections.back().InDisconnect()).c_str());
    }
  }

  //save any connections that were removed, any connections matching these regexes
  //and not matching any other regex combination will be disconnected in DisconnectPorts()
  for (vector<CPortConnection>::iterator oldit = m_connections.begin(); oldit != m_connections.end(); oldit++)
  {
    bool found = false;
    for (vector<CPortConnection>::iterator newit = connections.begin(); newit != connections.end(); newit++)
    {
      if (*oldit == *newit)
      {
        found = true;
        break;
      }
    }
    if (!found)
    {
      m_removed.push_back(*oldit);
      LogDebug("removed connection out:\"%s\" in:\"%s\"", oldit->Out().c_str(), oldit->In().c_str());
    }
  }

  //store the new connections
  m_connections.swap(connections);
}

CJSONGenerator* CPortConnector::ConnectionsToJSON()
{
  CJSONGenerator* generator = new CJSONGenerator(true);

  generator->MapOpen();
  generator->AddString("connections");
  generator->ArrayOpen();

  CLock lock(m_condition);
  for (vector<CPortConnection>::iterator it = m_connections.begin(); it != m_connections.end(); it++)
  {
    generator->MapOpen();

    generator->AddString("out");
    generator->AddString(it->Out());
    generator->AddString("in");
    generator->AddString(it->In());
    generator->AddString("outdisconnect");
    generator->AddBool(it->OutDisconnect());
    generator->AddString("indisconnect");
    generator->AddBool(it->InDisconnect());

    generator->MapClose();
  }
  lock.Leave();

  generator->ArrayClose();
  generator->MapClose();

  return generator;
}

CJSONGenerator* CPortConnector::PortIndexToJSON()
{
  CJSONGenerator* generator = new CJSONGenerator(true);

  generator->MapOpen();
  generator->AddString("index");
  CLock lock(m_condition);
  generator->AddInt(m_portindex);
  lock.Leave();
  generator->MapClose();

  return generator;
}

CJSONGenerator* CPortConnector::PortsToJSON()
{
  CLock lock(m_condition);

  CJSONGenerator* generator = new CJSONGenerator(true);

  generator->MapOpen();

  generator->AddString("index");
  generator->AddInt(m_portindex);
  generator->AddString("ports");
  generator->ArrayOpen();

  for (list<CJackPort>::iterator it = m_jackports.begin(); it != m_jackports.end(); it++)
  {
    generator->MapOpen();

    generator->AddString("name");
    generator->AddString(it->Name());
    generator->AddString("type");
    generator->AddString(it->TypeStr());

    generator->MapClose();
  }

  generator->ArrayClose();
  generator->MapClose();

  return generator;
}

#define MAXWAITINGTHREADS 100

CJSONGenerator* CPortConnector::PortsToJSON(const std::string& postjson, const std::string& source)
{
  string* error;
  CJSONElement* json = ParseJSON(postjson, error);
  auto_ptr<CJSONElement> jsonauto(json);

  //parse portindex and timeout, they should be JSON numbers
  //if they're invalid, these defaults will be used instead
  int64_t portindex = -1;
  int64_t timeout   = 0;
  if (error)
  {
    LogError("%s: %s", source.c_str(), error->c_str());
    delete error;
  }
  else
  {
    if (!json->IsMap())
    {
      LogError("%s: invalid value for root node: %s", source.c_str(), ToJSON(json).c_str());
    }
    else
    {
      JSONMap::iterator jsonportindex = json->AsMap().find("index");
      if (jsonportindex != json->AsMap().end())
      {
        if (jsonportindex->second->IsNumber())
          portindex = jsonportindex->second->ToInt64();
        else
          LogError("%s: invalid value for portindex: %s", source.c_str(), ToJSON(jsonportindex->second).c_str());
      }

      JSONMap::iterator jsontimeout = json->AsMap().find("timeout");
      if (jsontimeout != json->AsMap().end())
      {
        if (jsontimeout->second->IsNumber())
          timeout = jsontimeout->second->ToInt64();
        else
          LogError("%s: invalid value for timeout: %s", source.c_str(), ToJSON(jsontimeout->second).c_str());
      }
    }
  }

  CLock lock(m_condition);

  //limit the maximum number of threads waiting on the condition variable
  m_waitingthreads++;
  if (m_waitingthreads > MAXWAITINGTHREADS)
  {
    LogError("%i waiting threads, releasing one", m_waitingthreads);
    m_portindex++;
    m_condition.Signal();
  }

  //wait for the port index to change with the client requested timeout
  //the maximum timeout is one minute
  if (portindex == (int64_t)m_portindex && timeout > 0 && !m_stop)
    m_condition.Wait(Min(timeout, 60000) * 1000, m_portindex, (unsigned int)portindex);

  m_waitingthreads--;

  //if the portindex is the same, only send that, if it changed, send the ports too
  if (portindex == (int64_t)m_portindex)
    return PortIndexToJSON();
  else
    return PortsToJSON();
}

void CPortConnector::Process(bool& checkconnect, bool& checkdisconnect, bool& updateports)
{
  if (!checkconnect && !checkdisconnect && !updateports)
    return; //nothing to do

  //connect, connect ports and disconnect
  //since this client will never be active, if we leave it connected
  //then jackd will never signal any of the other bobdsp clients when it quits
  Connect();
  ProcessInternal(checkconnect, checkdisconnect, updateports);
  Disconnect();
}

void CPortConnector::Stop()
{
  CLock lock(m_condition);
  //interrupt all waiting httpserver threads so we don't hang on exit
  m_portindex++;
  m_stop = true;
  m_condition.Broadcast(); 
}

bool CPortConnector::ConnectInternal()
{
  if (m_connected)
    return true; //already connected

  LogDebug("Connecting portconnector to jack");

  m_client = jack_client_open("bobdsp_portconnector", JackNoStartServer, NULL);

  if (m_client == NULL)
  {
    if (m_wasconnected || g_printdebuglevel)
    {
      LogError("Portconnector error connecting to jackd: \"%s\"", GetErrno().c_str());
      m_wasconnected = false;
    }
    return false;
  }

  return true;
} 

void CPortConnector::ProcessInternal(bool& checkconnect, bool& checkdisconnect, bool& updateports)
{
  if (!m_connected)
  {
    //jackd is probably not running, clear the list of ports
    if (!m_jackports.empty())
    {
      CLock lock(m_condition);
      m_jackports.clear();
      m_portindex++;
      m_condition.Broadcast();
    }
    return;
  }

  //update the ports list
  if (updateports)
  {
    UpdatePorts();
    updateports = false;
  }

  //connect ports that match the regexes
  if (checkconnect)
    checkconnect = !ConnectPorts();

  //disconnect ports that don't match the regexes
  if (checkdisconnect)
    checkdisconnect = !DisconnectPorts();
}

bool CPortConnector::ConnectPorts()
{
  bool success = true;

  CLock lock(m_condition);

  for (list<CJackPort>::iterator inport = m_jackports.begin(); inport != m_jackports.end(); inport++)
  {
    //only use input ports here
    if (!(inport->Flags() & JackPortIsInput))
      continue;

    //check if the input port name matches an <in> regex
    for (vector<CPortConnection>::iterator it = m_connections.begin(); it != m_connections.end(); it++)
    {
      if (!it->InMatch(inport->Name()))
        continue;

      LogDebug("Regex \"%s\" matches input port \"%s\"", it->In().c_str(), inport->Name().c_str());

      //find output ports that match the <out> regex
      for (list<CJackPort>::iterator outport = m_jackports.begin(); outport != m_jackports.end(); outport++)
      {
        if (!(outport->Flags() & JackPortIsOutput) || !it->OutMatch(outport->Name()))
          continue;

        LogDebug("Regex \"%s\" matches output port \"%s\"", it->Out().c_str(), outport->Name().c_str());

        const jack_port_t* jackportout = jack_port_by_name(m_client, outport->Name().c_str());
        if (!jackportout)
          LogDebug("Can't find output port \"%s\", it probably deregistered", outport->Name().c_str());

        if (!jackportout || jack_port_connected_to(jackportout, inport->Name().c_str()))
          continue; //non existent port or already connected

        //if there's a match, connect
        int returnv = jack_connect(m_client, outport->Name().c_str(), inport->Name().c_str());
        if (returnv == 0)
        {
          Log("Connected port \"%s\" to port \"%s\"", outport->Name().c_str(), inport->Name().c_str());
        }
        else if (returnv != EEXIST)
        {
          LogError("Error %i connecting port \"%s\" to port \"%s\": \"%s\"",
              returnv, outport->Name().c_str(), inport->Name().c_str(), GetErrno().c_str());
          success = false;
        }
      }
    }
  }

  return success;
}

bool CPortConnector::DisconnectPorts()
{
  bool success = true;

  //build up a list of jack port connections
  vector< pair<string, string> > connectionlist; //pair.first = output port, pair.second = input port
  for (list<CJackPort>::iterator inport = m_jackports.begin(); inport != m_jackports.end(); inport++)
  {
    //only check input ports, since every connection is between an input and output port
    //we will get all connections this way
    if (!(inport->Flags() & JackPortIsInput))
        continue;

    const jack_port_t* jackportin = jack_port_by_name(m_client, inport->Name().c_str());
    if (!jackportin)
    {
      LogDebug("Can't find input port \"%s\", it probably deregistered", inport->Name().c_str());
      continue;
    }

    const char** connections = jack_port_get_all_connections(m_client, jackportin);
    if (connections)
    {
      for (const char** con = connections; *con != NULL; con++)
        connectionlist.push_back(make_pair(*con, inport->Name().c_str()));

      jack_free((void*)connections);
    }
  }

  CLock lock(m_condition);

  //match every connection to our regexes in connections
  for (vector< pair<string, string> >::iterator con = connectionlist.begin(); con != connectionlist.end(); con++)
  {
    LogDebug("\"%s\" is connected to \"%s\"", con->first.c_str(), con->second.c_str());

    bool matched    = false;
    bool disconnect = false;
    for (vector<CPortConnection>::iterator it = m_connections.begin(); it != m_connections.end(); it++)
    {
      bool outmatch;
      bool inmatch;
      MatchConnection(it, con, outmatch, inmatch, false);

      if (inmatch && outmatch)
      {
        matched = true;
        break; //full match, don't disconnect this one
      }
      else if ((outmatch && it->OutDisconnect()) || (inmatch && it->InDisconnect()))
      {
        //if we have a match on the input or output port of this connection, and the in or out disconnect is set
        //mark this connection for disconnection
        disconnect = true;
      }
    }

    //check if we need to disconnect a removed connection
    if (!matched && !disconnect)
    {
      for (vector<CPortConnection>::iterator it = m_removed.begin(); it != m_removed.end(); it++)
      {
        bool outmatch;
        bool inmatch;
        MatchConnection(it, con, outmatch, inmatch, true);

        if (inmatch && outmatch)
        {
          disconnect = true; //full match on a removed connection, disconnect
          break;
        }
      }
    }

    //disconnect connection if marked as such, and if there wasn't a full match
    if (disconnect && !matched)
    {
      int returnv = jack_disconnect(m_client, con->first.c_str(), con->second.c_str());
      if (returnv == 0)
      {
        Log("Disconnected port \"%s\" from port \"%s\"", con->first.c_str(), con->second.c_str());
      }
      else
      {
        LogError("Error %i disconnecting port \"%s\" from port \"%s\": \"%s\"",
                  returnv, con->first.c_str(), con->second.c_str(), GetErrno().c_str());
        success = false;
      }
    }
  }

  m_removed.clear();

  return success;
}

void CPortConnector::MatchConnection(vector<CPortConnection>::iterator& it, vector< pair<string, string> >::iterator& con,
                                     bool& outmatch, bool& inmatch, bool removed)
{
  outmatch = it->OutMatch(con->first);
  if (outmatch)
    LogDebug("Regex \"%s\" matches%soutput port \"%s\"", it->Out().c_str(), removed ? " removed " : " ", con->first.c_str());

  inmatch = it->InMatch(con->second);
  if (inmatch)
    LogDebug("Regex \"%s\" matches%sinput port \"%s\"", it->In().c_str(), removed ? " removed " : " ", con->second.c_str());
}

void CPortConnector::UpdatePorts()
{
  std::list<CJackPort> jackports;

  //jack_get_ports returns NULL when there are no ports
  const char** ports = jack_get_ports(m_client, NULL, NULL, 0);
  if (ports)
  {
    for (const char** portname = ports; *portname != NULL; portname++)
    {
      const jack_port_t* jackport = jack_port_by_name(m_client, *portname);
      if (!jackport)
      {
        LogError("Unable to get flags from port \"%s\"", *portname);
        continue;
      }

      int portflags = jack_port_flags(jackport);

      LogDebug("Found %s port \"%s\"", portflags & JackPortIsInput ? "input" : "output", *portname);
      jackports.push_back(CJackPort(*portname, portflags));
    }
    jackports.sort();
    jack_free((void*)ports);
  }

  //check if the list really changed, it might not if the main loop got a delayed event from a jack client
  bool changed = jackports.size() != m_jackports.size();
  if (!changed)
  {
    list<CJackPort>::iterator oldport = m_jackports.begin();
    list<CJackPort>::iterator newport = jackports.begin();

    while (oldport != m_jackports.end())
    {
      if (*oldport != *newport)
      {
        changed = true;
        break;
      }
      oldport++;
      newport++;
    }
  }

  //if the list of connections really changed, update and signal threads waiting on m_condition
  if (changed)
  {
    CLock lock(m_condition);
    m_jackports.swap(jackports);
    m_portindex++;
    m_condition.Broadcast();
  }
  else
  {
    LogDebug("Port list update requested but list did not change");
  }
}

