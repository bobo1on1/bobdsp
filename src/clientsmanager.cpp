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
#include "util/log.h"
#include "util/lock.h"
#include "util/JSON.h"
#include "util/timeutils.h"
#include "clientsmanager.h"
#include "bobdsp.h"

using namespace std;

CClientsManager::CClientsManager(CBobDSP& bobdsp):
  m_bobdsp(bobdsp)
{
}

CClientsManager::~CClientsManager()
{
}

void CClientsManager::Stop()
{
  Log("Stopping %zu jack client(s)", m_clients.size());
  for (vector<CJackClient*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
    delete *it;
}

/* load client settings from clients.xml
   it may look like this:

<clients>
  <client>
    <name>Audio limiter</name>
    <label>fastLookaheadLimiter</label>
    <uniqueid>1913</uniqueid>
    <instances>1</instances>
    <pregain>1.0</pregain>
    <postgain>1.0</postgain>
    <port>
      <name>Input gain (dB)</name>
      <value>0</value>
    </port>
    <port>
      <name>Limit (dB)</name>
      <value>0</value>
    </port>
    <port>
      <name>Release time (s)</name>
      <value>0.1</value>
    </port>
  </client>
</clients>

  <name> is the name for the jack client, it's not related to the name of the ladspa plugin
  <label> is the label of the ladspa plugin
  <uniqueid> is the unique id of the ladspa plugin
  <instances> sets the number of instances for the plugin, by increasing instances you can process more audio channels
  <pregain> is the audio gain for the ladspa input ports
  <postgain> is the audio gain for the ladspa output ports
*/

void CClientsManager::ClientsFromXML(TiXmlElement* root)
{
  for (TiXmlElement* client = root->FirstChildElement("client"); client != NULL; client = client->NextSiblingElement("client"))
  {
    LogDebug("Read <client> element");

    bool loadfailed = false;

    LOADELEMENT(client, name, MANDATORY);
    LOADELEMENT(client, label, MANDATORY);
    LOADINTELEMENT(client, uniqueid, MANDATORY, 0, POSTCHECK_NONE);
    LOADINTELEMENT(client, instances, OPTIONAL, 1, POSTCHECK_ONEORHIGHER);
    LOADFLOATELEMENT(client, pregain, OPTIONAL, 1.0, POSTCHECK_NONE);
    LOADFLOATELEMENT(client, postgain, OPTIONAL, 1.0, POSTCHECK_NONE);

    if (loadfailed || instances_parsefailed || pregain_parsefailed || postgain_parsefailed)
      continue;

    LogDebug("name:\"%s\" label:\"%s\" uniqueid:%" PRIi64 " instances:%" PRIi64 " pregain:%.2f postgain:%.2f",
             name->GetText(), label->GetText(), uniqueid_p, instances_p, pregain_p, postgain_p);

    vector<portvalue> portvalues;
    if (!LoadPortsFromClient(client, portvalues))
      continue;

    CLadspaPlugin* ladspaplugin = m_bobdsp.PluginManager().GetPlugin(uniqueid_p, label->GetText());
    if (ladspaplugin)
    {
      LogDebug("Found matching ladspa plugin in %s", ladspaplugin->FileName().c_str());
    }
    else
    {
      LogError("Did not find matching ladspa plugin for \"%s\" label \"%s\" uniqueid %" PRIi64,
               name->GetText(), label->GetText(), uniqueid_p);
      continue;
    }

    //check if all ports from the xml match the ladspa plugin
    bool allportsok = true;
    for (vector<portvalue>::iterator it = portvalues.begin(); it != portvalues.end(); it++)
    {
      bool found = false;
      for (unsigned long port = 0; port < ladspaplugin->PortCount(); port++)
      {
        if (ladspaplugin->IsControlInput(port) && it->first == ladspaplugin->PortName(port))
        {
          LogDebug("Found port \"%s\"", it->first.c_str());
          found = true;
          break;
        }
      }
      if (!found)
      {
        LogError("Did not find port \"%s\" in plugin \"%s\"", it->first.c_str(), ladspaplugin->Label());
        allportsok = false;
      }
    }

    //check if all control input ports are mapped
    for (unsigned long port = 0; port < ladspaplugin->PortCount(); port++)
    {
      if (ladspaplugin->IsControlInput(port))
      {
        bool found = false;
        for (vector<portvalue>::iterator it = portvalues.begin(); it != portvalues.end(); it++)
        {
          if (it->first == ladspaplugin->PortName(port))
          {
            found = true;
            break;
          }
        }
        if (!found)
        {
          LogError("Port \"%s\" of plugin \"%s\" is not mapped", ladspaplugin->PortName(port), ladspaplugin->Label());
          allportsok = false;
        }
      }

      if (!ladspaplugin->PortDescriptorSanityCheck(port))
        allportsok = false;
    }

    if (!allportsok)
      continue;

    CJackClient* jackclient = new CJackClient(ladspaplugin, name->GetText(), instances_p, pregain_p, postgain_p, portvalues);
    m_clients.push_back(jackclient);
  }
}

bool CClientsManager::LoadPortsFromClient(TiXmlElement* client, std::vector<portvalue>& portvalues)
{
  bool success = true;

  for (TiXmlElement* port = client->FirstChildElement("port"); port != NULL; port = port->NextSiblingElement("port"))
  {
    LogDebug("Read <port> element");

    bool loadfailed = false;

    LOADELEMENT(port, name, MANDATORY);
    LOADFLOATELEMENT(port, value, MANDATORY, 0, POSTCHECK_NONE);

    if (loadfailed)
    {
      success = false;
      continue;
    }

    LogDebug("name:\"%s\" value:%.2f", name->GetText(), value_p);
    portvalues.push_back(make_pair(name->GetText(), value_p));
  }

  return success;
}

std::string CClientsManager::ClientsToJSON()
{
  JSON::CJSONGenerator generator;

  generator.MapOpen();
  generator.AddString("clients");
  generator.ArrayOpen();

  for (vector<CJackClient*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
  {
    generator.MapOpen();

    generator.AddString("name");
    generator.AddString((*it)->Name());
    generator.AddString("instances");
    generator.AddInt((*it)->NrInstances());
    generator.AddString("pregain");
    generator.AddDouble((*it)->PreGain());
    generator.AddString("postgain");
    generator.AddDouble((*it)->PostGain());

    generator.AddString("controls");
    generator.ArrayOpen();
    const vector<portvalue>& controls = (*it)->ControlInputs();
    for (vector<portvalue>::const_iterator control = controls.begin(); control != controls.end(); control++)
    {
      generator.MapOpen();

      generator.AddString("name");
      generator.AddString(control->first);
      generator.AddString("value");
      generator.AddDouble(control->second);

      //add the port description of this port
      CLadspaPlugin* plugin = (*it)->Plugin();
      long port = plugin->PortByName(control->first);
      m_bobdsp.PluginManager().PortRangeDescriptionToJSON(generator, plugin, port);

      generator.MapClose();
    }
    generator.ArrayClose();

    generator.AddString("plugin");
    generator.MapOpen();
    generator.AddString("label");
    generator.AddString((*it)->Plugin()->Label());
    generator.AddString("uniqueid");
    generator.AddInt((*it)->Plugin()->UniqueID());
    generator.MapClose();

    generator.MapClose();
  }

  generator.ArrayClose();
  generator.MapClose();

  return generator.ToString();
}

bool CClientsManager::Process(bool& triedconnect, bool& allconnected, int64_t lastconnect)
{
  bool connected = false;

  for (vector<CJackClient*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
  {
    //check if the jack thread has failed
    if ((*it)->ExitStatus())
    {
      LogError("Client \"%s\" exited with code %i reason: \"%s\"",
               (*it)->Name().c_str(), (int)(*it)->ExitStatus(), (*it)->ExitReason().c_str());
      (*it)->Disconnect();
    }

    //keep trying to connect
    //only try to connect at the interval to prevent hammering jackd
    if (!(*it)->IsConnected())
    {
      allconnected = false;
      if (GetTimeUs() - lastconnect >= CONNECTINTERVAL)
      {
        triedconnect = true;
        if ((*it)->Connect())
        {
          connected = true;

          //update samplerate
          if ((*it)->Samplerate() != 0)
            m_bobdsp.PluginManager().SetSamplerate((*it)->Samplerate());
        }
      }
    }
  }

  return connected;
}

int CClientsManager::ClientPipes(pollfd* fds)
{
  int nrfds = 0;
  for (vector<CJackClient*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
  {
    int pipe = (*it)->MsgPipe();
    if (pipe != -1)
    {
      fds[nrfds].fd     = pipe;
      fds[nrfds].events = POLLIN;
      nrfds++;
    }
  }

  return nrfds;
}

void CClientsManager::ProcessMessages(bool& checkconnect, bool& checkdisconnect, bool& updateports)
{
  //check events of all clients, instead of just the ones that poll() returned on
  //since in case of a jack event, every client sends a message
  for (int i = 0; i < 2; i++)
  {
    bool gotmessage = false;
    for (vector<CJackClient*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
    {
      uint8_t msg;
      while ((msg = (*it)->GetMessage()) != MsgNone)
      {
        LogDebug("got message %s from client \"%s\"", MsgToString(msg), (*it)->Name().c_str());
        if (msg == MsgExited)
          updateports = true;
        else if (msg == MsgPortRegistered)
          updateports = checkconnect = true;
        else if (msg == MsgPortDeregistered)
          updateports = true;
        else if (msg == MsgPortConnected)
          checkdisconnect = true;
        else if (msg == MsgPortDisconnected)
          checkconnect = true;

        gotmessage = true;
      }
    }

    //in case of a jack event, every connected client sends a message
    //to make sure we get them all in one go, if a message from one client is received
    //wait a millisecond, then check all clients for messages again
    if (gotmessage && i == 0)
      USleep(1000);
    else
      break;
  }
}

