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

bool CPortConnector::ConnectionsFromXML(TiXmlElement* root)
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

    if (valid)
      connections.push_back(connection);
  }

  if (valid)
  {
    //save any connections that were removed, so we can disconnect those ports
    CLock lock(m_mutex);
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
    m_connections = connections;
  }

  return valid;
}

bool CPortConnector::ConnectionsFromJSON(const std::string& json)
{
  TiXmlElement* root = JSONXML::JSONToXML(json);

  bool success = true;

  TiXmlNode* connections = root->FirstChildElement("connections");
  if (connections && connections->Type() == TiXmlNode::TINYXML_ELEMENT)
    success = ConnectionsFromXML(connections->ToElement());

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

#define YAJLSTRING(str) (const unsigned char*)(str), strlen((str))

std::string CPortConnector::ConnectionsToJSON()
{
  string json;

#if YAJL_MAJOR == 2
  yajl_gen handle = yajl_gen_alloc(NULL);
  yajl_gen_config(handle, yajl_gen_beautify, 1);
  yajl_gen_config(handle, yajl_gen_indent_string, "  ");
#else
  yajl_gen_config yajlconfig;
  yajlconfig.beautify = 1;
  yajlconfig.indentString = "  ";
  yajl_gen handle = yajl_gen_alloc(&yajlconfig, NULL);
#endif

  yajl_gen_map_open(handle);
  yajl_gen_string(handle, YAJLSTRING("connections"));
  yajl_gen_map_open(handle);
  yajl_gen_string(handle, YAJLSTRING("connection"));
  yajl_gen_array_open(handle);

  CLock lock(m_mutex);
  for (vector<portconnection>::iterator it = m_connections.begin(); it != m_connections.end(); it++)
  {
    yajl_gen_map_open(handle);
    yajl_gen_string(handle, YAJLSTRING("out"));
    yajl_gen_string(handle, YAJLSTRING(it->out.c_str()));
    yajl_gen_string(handle, YAJLSTRING("in"));
    yajl_gen_string(handle, YAJLSTRING(it->in.c_str()));
    yajl_gen_string(handle, YAJLSTRING("disconnect"));
    yajl_gen_string(handle, YAJLSTRING(it->DisconnectStr()));

    yajl_gen_map_close(handle);
  }
  lock.Leave();

  yajl_gen_array_close(handle);
  yajl_gen_map_close(handle);
  yajl_gen_map_close(handle);

  const unsigned char* str;
  YAJLSTRINGLEN length;
  yajl_gen_get_buf(handle, &str, &length);
  json = string((const char *)str, length);

  yajl_gen_clear(handle);
  yajl_gen_free(handle);

  return json;
}

TiXmlElement* CPortConnector::ConnectionsToXML()
{
  TiXmlElement* root = new TiXmlElement("connections");

  CLock lock(m_mutex);
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

void CPortConnector::Process(bool& checkconnect, bool& checkdisconnect)
{
  if (!checkconnect && !checkdisconnect)
    return; //nothing to do

  //connect, connect ports and disconnect
  //since this client will never be active, if we leave it connected
  //then jackd will never signal any of the other bobdsp clients when it quits
  Connect();
  ProcessInternal(checkconnect, checkdisconnect);
  Disconnect();
}

bool CPortConnector::ConnectInternal()
{
  if (m_connected)
    return true; //already connected

  LogDebug("Connecting portconnector to jack");

  m_client = jack_client_open("bobdsp_portconnector", JackNoStartServer, NULL);

  if (m_client == NULL)
  {
    LogError("Portconnector error connecting to jackd: \"%s\"", GetErrno().c_str());
    return false;
  }

  return true;
} 

void CPortConnector::ProcessInternal(bool& checkconnect, bool& checkdisconnect)
{
  if (!m_connected)
    return;

  const char** ports = jack_get_ports(m_client, NULL, NULL, 0);

  if (checkconnect)
  {
    ConnectPorts(ports);   //connect ports that match the regexes
    checkconnect = false;
  }

  if (checkdisconnect)
  {
    DisconnectPorts(ports);//disconnect ports that don't match the regexes
    checkdisconnect = false;
  }

  jack_free((void*)ports);
}

void CPortConnector::ConnectPorts(const char** ports)
{
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
    CLock lock(m_mutex);
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
          Log("Connected port \"%s\" to port \"%s\"", *outport, *inport);
        else if (returnv != EEXIST)
          LogError("Error %i connecting port \"%s\" to port \"%s\": \"%s\"",
              returnv, *outport, *inport, GetErrno().c_str());
      }
    }
  }
}

void CPortConnector::DisconnectPorts(const char** ports)
{
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
  CLock lock(m_mutex);
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
    lock.Enter();
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
    lock.Leave();

    //check if we need to disconnect a removed connection
    if (!matched)
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
        Log("Disconnected port \"%s\" from port \"%s\"", con->first.c_str(), con->second.c_str());
      else
        LogError("Error %i disconnecting port \"%s\" from port \"%s\": \"%s\"",
                  returnv, con->first.c_str(), con->second.c_str(), GetErrno().c_str());
    }
  }
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

