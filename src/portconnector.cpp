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
#include <list>
#include <errno.h>
#include <pcrecpp.h>
#include "portconnector.h"
#include "util/log.h"

using namespace std;

CPortConnector::CPortConnector()
{
  m_client = NULL;
  m_connected = false;
}

CPortConnector::~CPortConnector()
{
  Disconnect();
}

void CPortConnector::AddConnection(portconnection& connection)
{
  m_connections.push_back(connection);
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

void CPortConnector::Process(bool& portregistered, bool& portconnected)
{
  if (!portregistered && !portconnected)
    return; //nothing to do

  //connect, connect ports and disconnect
  //since this client will never be active, if we leave it connected
  //then jackd will never signal any of the other bobdsp clients when it quits
  Connect();
  ProcessInternal(portregistered, portconnected);
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

void CPortConnector::ProcessInternal(bool& portregistered, bool& portconnected)
{
  if (!m_connected)
    return;

  const char** ports = jack_get_ports(m_client, NULL, NULL, 0);

  if (portregistered)
  {
    ConnectPorts(ports);   //connect ports that match the regexes
    portregistered = false;
  }

  if (portconnected)
  {
    DisconnectPorts(ports);//disconnect ports that don't match the regexes
    portconnected = false;
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

    //check if the input port name matches an <in> regex
    for (vector<portconnection>::iterator it = m_connections.begin(); it != m_connections.end(); it++)
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
  list< pair<string, string> > connectionlist; //pair.first = output port, pair.second = input port
  for (const char** portname = ports; *portname != NULL; portname++)
  {
    const jack_port_t* jackport = jack_port_by_name(m_client, *portname);
    int  portflags = jack_port_flags(jackport);
    bool isinput   = (portflags & JackPortIsInput) == JackPortIsInput;

    const char** connections = jack_port_get_all_connections(m_client, jackport);
    if (connections)
    {
      if (isinput)
      {
        for (const char** con = connections; *con != NULL; con++)
          connectionlist.push_back(make_pair(*con, *portname));
      }
      else
      {
        for (const char** con = connections; *con != NULL; con++)
          connectionlist.push_back(make_pair(*portname, *con));
      }
      jack_free((void*)connections);
    }
  }

  //every connection will be in the list twice, get rid of the copies
  connectionlist.sort();
  connectionlist.unique();

  //match every connection to our regexes in m_connections
  for (list< pair<string, string> >::iterator con = connectionlist.begin(); con != connectionlist.end(); con++)
  {
    LogDebug("\"%s\" is connected to \"%s\"", con->first.c_str(), con->second.c_str());

    bool matched    = false;
    bool disconnect = false;
    for (vector<portconnection>::iterator it = m_connections.begin(); it != m_connections.end(); it++)
    {
      pcrecpp::RE outre(it->out);
      bool outmatch = outre.FullMatch(con->first);
      if (outmatch)
        LogDebug("Regex \"%s\" matches output port \"%s\"", it->out.c_str(), con->first.c_str());

      pcrecpp::RE inre(it->in);
      bool inmatch = inre.FullMatch(con->second);
      if (inmatch)
        LogDebug("Regex \"%s\" matches input port \"%s\"", it->in.c_str(), con->second.c_str());

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
      }
    }
  }
}

