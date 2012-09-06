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
#include <pcrecpp.h>

#include "bobdsp.h"
#include "portconnector.h"
#include "util/log.h"
#include "util/lock.h"

using namespace std;

CPortConnector::CPortConnector(CBobDSP& bobdsp) :
  m_bobdsp(bobdsp)
{
  m_client = NULL;
  m_connected = false;
  m_wasconnected = true;
  m_stop = false;
  m_portindex = 0;
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

bool CPortConnector::ConnectionsFromXML(TiXmlElement* root, bool strict)
{
  std::vector<portconnection> connections;

  bool valid = true;

  for (TiXmlElement* xmlconnection = root->FirstChildElement("connection"); xmlconnection != NULL;
       xmlconnection = xmlconnection->NextSiblingElement("connection"))
  {
    LogDebug("Read <connection> element");

    bool loadfailed = false;

    LOADELEMENT(xmlconnection, in, MANDATORY);
    LOADELEMENT(xmlconnection, out, MANDATORY);
    LOADELEMENT(xmlconnection, disconnect, OPTIONAL);

    if (loadfailed)
    {
      valid = false;
      continue;
    }

    LogDebug("in:\"%s\" out:\"%s\"", in->GetText(), out->GetText());

    portconnection connection;
    connection.in = in->GetText();
    connection.out = out->GetText();
    connection.indisconnect = false;
    connection.outdisconnect = false;

    if (!disconnect_loadfailed)
    {
      LogDebug("disconnect: \"%s\"", disconnect->GetText());
      string strdisconnect = disconnect->GetText();
      if (strdisconnect == "in")
        connection.indisconnect = true;
      else if (strdisconnect == "out")
        connection.outdisconnect = true;
      else if (strdisconnect == "both")
        connection.indisconnect = connection.outdisconnect = true;
      else if (strdisconnect != "none")
      {
        LogError("Invalid value \"%s\" for element <disconnect>", strdisconnect.c_str());
        valid = false;
      }
    }

    connections.push_back(connection);
  }

  if (!strict || valid)
  {
    //save any connections that were removed, so we can disconnect those ports
    CLock lock(m_condition);
    for (vector<portconnection>::iterator oldit = m_connections.begin(); oldit != m_connections.end(); oldit++)
    {
      bool found = false;
      for (vector<portconnection>::iterator newit = connections.begin(); newit != connections.end(); newit++)
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
        LogDebug("removed connection out:\"%s\" in:\"%s\" disconnect:\"%s\"",
            oldit->out.c_str(), oldit->in.c_str(), oldit->DisconnectStr());
      }
    }
    
    //store the new connections
    m_connections = connections;
  }

  return valid;
}

bool CPortConnector::ConnectionsFromJSON(const std::string& json)
{
  TiXmlElement* root = JSON::JSONToXML(json);

  bool success = true;

  CLock lock(m_condition);

  TiXmlNode* connections = root->FirstChildElement("connections");
  if (connections && connections->Type() == TiXmlNode::TINYXML_ELEMENT)
    success = ConnectionsFromXML(connections->ToElement(), true);

  bool loadfailed = false;

  LOADBOOLELEMENT(root, save, OPTIONAL, false, POSTCHECK_NONE);
  if (success && save_p)
    m_bobdsp.SaveConnectionsToFile(ConnectionsToXML());

  LOADBOOLELEMENT(root, reload, OPTIONAL, false, POSTCHECK_NONE);
  if (reload_p)
    m_bobdsp.LoadConnectionsFromFile();

  delete root;
  return success;
}

std::string CPortConnector::ConnectionsToJSON()
{
  JSON::CJSONGenerator generator;

  generator.MapOpen();
  generator.AddString("connections");
  generator.MapOpen();
  generator.AddString("connection");
  generator.ArrayOpen();

  CLock lock(m_condition);
  for (vector<portconnection>::iterator it = m_connections.begin(); it != m_connections.end(); it++)
  {
    generator.MapOpen();

    generator.AddString("out");
    generator.AddString(it->out);
    generator.AddString("in");
    generator.AddString(it->in);
    generator.AddString("disconnect");
    generator.AddString(it->DisconnectStr());

    generator.MapClose();
  }
  lock.Leave();

  generator.ArrayClose();
  generator.MapClose();
  generator.MapClose();

  return generator.ToString();
}

TiXmlElement* CPortConnector::ConnectionsToXML()
{
  TiXmlElement* root = new TiXmlElement("connections");

  CLock lock(m_condition);
  for (vector<portconnection>::iterator it = m_connections.begin(); it != m_connections.end(); it++)
  {
    TiXmlElement out("out");
    out.InsertEndChild(TiXmlText(it->out));

    TiXmlElement in("in");
    in.InsertEndChild(TiXmlText(it->in));

    TiXmlElement disconnect("disconnect");
    disconnect.InsertEndChild(TiXmlText(it->DisconnectStr()));

    TiXmlElement connection("connection");
    connection.InsertEndChild(out);
    connection.InsertEndChild(in);
    connection.InsertEndChild(disconnect);

    root->InsertEndChild(connection);
  }

  return root;
}

std::string CPortConnector::PortsToJSON()
{
  CLock lock(m_condition);

  JSON::CJSONGenerator generator;

  generator.MapOpen();

  generator.AddString("ports");
  generator.MapOpen();
  generator.AddString("port");
  generator.ArrayOpen();

  for (list<CJackPort>::iterator it = m_jackports.begin(); it != m_jackports.end(); it++)
  {
    generator.MapOpen();

    generator.AddString("name");
    generator.AddString(it->name);
    generator.AddString("type");
    generator.AddString(it->TypeStr());

    generator.MapClose();
  }

  generator.ArrayClose();
  generator.AddString("portindex");
  generator.AddInt(m_portindex);
  generator.MapClose();
  generator.MapClose();

  return generator.ToString();
}

std::string CPortConnector::PortsToJSON(std::string& postjson)
{
  TiXmlElement* root = JSON::JSONToXML(postjson);

  bool loadfailed = false;

  LOADINTELEMENT(root, timeout, OPTIONAL, 0, POSTCHECK_ZEROORHIGHER);
  LOADINTELEMENT(root, portindex, OPTIONAL, 0, POSTCHECK_NONE);

  //wait for the port index to change with the client requested timeout
  //the maximum timeout is one hour
  CLock lock(m_condition);
  if (portindex_p == m_portindex && timeout_p > 0 && !m_stop)
    m_condition.Wait(Min(timeout_p, 3600 * 1000) * 1000);

  delete root;
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
  m_stop = true;
  //interrupt all waiting httpserver threads so we don't hang on exit
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

  const char** ports = jack_get_ports(m_client, NULL, NULL, 0);

  //connect ports that match the regexes
  if (checkconnect)
    checkconnect = !ConnectPorts(ports);

  //disconnect ports that don't match the regexes
  if (checkdisconnect)
    checkdisconnect = !DisconnectPorts(ports);

  //update the ports list
  if (updateports)
  {
    UpdatePorts(ports);
    updateports = false;
  }

  jack_free((void*)ports);
}

bool CPortConnector::ConnectPorts(const char** ports)
{
  bool success = true;

  for (const char** portname = ports; *portname != NULL; portname++)
  {
    //find input ports
    const jack_port_t* jackport = jack_port_by_name(m_client, *portname);
    int portflags = jack_port_flags(jackport);
    if (portflags & JackPortIsInput)
    {
      LogDebug("Found input port \"%s\"", *portname);
    }
    else
    {
      LogDebug("Found output port \"%s\"", *portname);
      continue;
    }

    const char** inport = portname;

    //copy the connections, so we don't deadlock the httpserver if this thread hangs when talking to jackd
    CLock lock(m_condition);
    vector<portconnection> connections = m_connections;
    lock.Leave();

    //check if the input port name matches an <in> regex
    for (vector<portconnection>::iterator it = connections.begin(); it != connections.end(); it++)
    {
      pcrecpp::RE inputre(it->in);
      if (!inputre.FullMatch(*inport))
        continue;

      LogDebug("Regex \"%s\" matches input port \"%s\"", it->in.c_str(), *inport);

      //find output ports that match the <out> regex
      pcrecpp::RE outputre(it->out);
      for (const char** outport = ports; *outport != NULL; outport++)
      {
        const jack_port_t* jackportout = jack_port_by_name(m_client, *outport);
        int outportflags = jack_port_flags(jackportout);
        if (!(outportflags & JackPortIsOutput) || !outputre.FullMatch(*outport))
          continue;

        LogDebug("Regex \"%s\" matches output port \"%s\"", it->out.c_str(), *outport);

        if (jack_port_connected_to(jackportout, *inport))
          continue; //alread connected

        //if there's a match, connect
        int returnv = jack_connect(m_client, *outport, *inport);
        if (returnv == 0)
        {
          Log("Connected port \"%s\" to port \"%s\"", *outport, *inport);
        }
        else if (returnv != EEXIST)
        {
          LogError("Error %i connecting port \"%s\" to port \"%s\": \"%s\"",
              returnv, *outport, *inport, GetErrno().c_str());
          success = false;
        }
      }
    }
  }

  return success;
}

bool CPortConnector::DisconnectPorts(const char** ports)
{
  bool success = true;

  //build up a list of jack port connections
  vector< pair<string, string> > connectionlist; //pair.first = output port, pair.second = input port
  for (const char** portname = ports; *portname != NULL; portname++)
  {
    const jack_port_t* jackport = jack_port_by_name(m_client, *portname);
    int  portflags = jack_port_flags(jackport);
    
    //only check input ports, since every connection is between an input and output port
    //we will get all connections this way
    if (!((portflags & JackPortIsInput) == JackPortIsInput))
        continue;

    const char** connections = jack_port_get_all_connections(m_client, jackport);
    if (connections)
    {
      for (const char** con = connections; *con != NULL; con++)
        connectionlist.push_back(make_pair(*con, *portname));

      jack_free((void*)connections);
    }
  }

  //copy the connections and removed connections, this is to avoid a race condition
  CLock lock(m_condition);
  vector<portconnection> connections = m_connections;
  vector<portconnection> removed = m_removed;
  m_removed.clear();
  lock.Leave();

  //match every connection to our regexes in connections
  for (vector< pair<string, string> >::iterator con = connectionlist.begin(); con != connectionlist.end(); con++)
  {
    LogDebug("\"%s\" is connected to \"%s\"", con->first.c_str(), con->second.c_str());

    bool matched    = false;
    bool disconnect = false;
    for (vector<portconnection>::iterator it = connections.begin(); it != connections.end(); it++)
    {
      bool outmatch;
      bool inmatch;
      MatchConnection(it, con, outmatch, inmatch, false);

      if (inmatch && outmatch)
      {
        matched = true;
        break; //full match, don't disconnect this one
      }
      else if ((inmatch && it->indisconnect) || (outmatch && it->outdisconnect))
      {
        //if we have a match on the input or output port of this connection, and the in or out disconnect is set
        //mark this connection for disconnection
        disconnect = true;
      }
    }

    //check if we need to disconnect a removed connection
    if (!matched && !disconnect)
    {
      for (vector<portconnection>::iterator it = removed.begin(); it != removed.end(); it++)
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

  return success;
}

void CPortConnector::MatchConnection(vector<portconnection>::iterator& it, vector< pair<string, string> >::iterator& con,
                                     bool& outmatch, bool& inmatch, bool removed)
{
  pcrecpp::RE outre(it->out);
  outmatch = outre.FullMatch(con->first);
  if (outmatch)
    LogDebug("Regex \"%s\" matches%soutput port \"%s\"", it->out.c_str(), removed ? " removed " : " ", con->first.c_str());

  pcrecpp::RE inre(it->in);
  inmatch = inre.FullMatch(con->second);
  if (inmatch)
    LogDebug("Regex \"%s\" matches%sinput port \"%s\"", it->in.c_str(), removed ? " removed " : " ", con->second.c_str());
}

void CPortConnector::UpdatePorts(const char** ports)
{
  CLock lock(m_condition);
  m_jackports.clear();

  for (const char** portname = ports; *portname != NULL; portname++)
  {
    const jack_port_t* jackport = jack_port_by_name(m_client, *portname);
    int  portflags = jack_port_flags(jackport);

    m_jackports.push_back(CJackPort(*portname, portflags));
  }
  m_jackports.sort();

  m_portindex++;
  m_condition.Broadcast();
}

