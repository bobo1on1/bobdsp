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

#include <memory>

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
  CLock lock(m_mutex);

  Log("Stopping %zu jack client(s)", m_clients.size());
  for (vector<CJackClient*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
    delete *it;
}

void CClientsManager::LoadSettingsFromFile(const std::string& filename)
{
  Log("Loading client settings from %s", filename.c_str());

  string* error;
  CJSONElement* json = ParseJSONFile(filename, error);
  auto_ptr<CJSONElement> jsonauto(json);

  if (error)
  {
    LogError("%s: %s", filename.c_str(), error->c_str());
    delete error;
    return;
  }

  CLock lock(m_mutex);
  LoadSettings(json, filename);
}

CJSONGenerator* CClientsManager::LoadSettingsFromString(const std::string& strjson, const std::string& source,
                                                        bool returnsettings /*= false*/)
{
  string* error;
  CJSONElement* json = ParseJSON(strjson, error);
  auto_ptr<CJSONElement> jsonauto(json);

  CLock lock(m_mutex);

  if (error)
  {
    LogError("%s: %s", source.c_str(), error->c_str());
    delete error;
  }
  else
  {
    LoadSettings(json, source);
  }

  if (returnsettings)
    return ClientsToJSON();
  else
    return NULL;
}

void CClientsManager::LoadSettings(CJSONElement* json, const std::string& source)
{
  if (!json->IsMap())
  {
    LogError("%s: invalid value for root node: %s", source.c_str(), ToJSON(json).c_str());
    return;
  }

  JSONMap::iterator clients = json->AsMap().find("clients");
  if (clients == json->AsMap().end())
  {
    LogError("%s: \"clients\" array missing", source.c_str());
    return;
  }
  else if (!clients->second->IsArray())
  {
    LogError("%s: invalid value for \"clients\": %s", source.c_str(), ToJSON(clients->second).c_str());
    return;
  }

  for (JSONArray::iterator it = clients->second->AsArray().begin(); it != clients->second->AsArray().end(); it++)
    LoadClientSettings(*it, source + ": ");
}

void CClientsManager::LoadClientSettings(CJSONElement* jsonclient, std::string source)
{
  if (!jsonclient->IsMap())
  {
    LogError("%sinvalid value for client: %s", source.c_str(), ToJSON(jsonclient).c_str());
    return;
  }

  JSONMap& client = jsonclient->AsMap();

  JSONMap::iterator name = client.find("name");
  if (name == client.end())
  {
    LogError("%sclient has no name", source.c_str());
    return;
  }
  else if (!name->second->IsString())
  {
    LogError("%sinvalid value for client name: %s", source.c_str(), ToJSON(name->second).c_str());
    return;
  }

  LogDebug("Loading settings for client \"%s\"", name->second->AsString().c_str());

  //print the client name in the source
  source += "client \"";
  source += name->second->AsString();
  source += "\" ";

  for (vector<CJackClient*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
  {
    if ((*it)->Name() == name->second->AsString())
    {
      LogError("%salready exists", source.c_str());
      return;
    }
  }

  int iinstances = 1;

  JSONMap::iterator instances = client.find("instances");
  if (instances != client.end())
  {
    if (!instances->second->IsNumber() || instances->second->ToInt64() <= 0)
    {
      LogError("%sinvalid value for instances: %s", source.c_str(), ToJSON(instances->second).c_str());
      return;
    }
    else
      iinstances = instances->second->ToInt64();
  }

  double fpregain = 1.0;
  double fpostgain = 1.0;

  JSONMap::iterator pregain = client.find("pregain");
  if (pregain != client.end())
  {
    if (!pregain->second->IsNumber())
    {
      LogError("%sinvalid value for pregain: %s", source.c_str(), ToJSON(pregain->second).c_str());
      return;
    }
    else
      fpregain = pregain->second->ToDouble();
  }

  JSONMap::iterator postgain = client.find("postgain");
  if (postgain != client.end())
  {
    if (!postgain->second->IsNumber())
    {
      LogError("%sinvalid value for postgain: %s", source.c_str(), ToJSON(postgain->second).c_str());
      return;
    }
    else
      fpostgain = postgain->second->ToDouble();
  }

  LogDebug("%sinstances:%i pregain:%.3f postgain:%.3f",
           source.c_str(), iinstances, fpregain, fpostgain);

  CLadspaPlugin* ladspaplugin = LoadPlugin(source, client);
  if (ladspaplugin == NULL)
    return;

  std::vector<controlvalue> controlvalues;
  if (!LoadControls(source, client, controlvalues))
    return;

  if (!CheckControls(source, ladspaplugin, controlvalues))
    return;

  CJackClient* jackclient = new CJackClient(ladspaplugin, name->second->AsString(), iinstances,
                                            fpregain, fpostgain, controlvalues);
  m_clients.push_back(jackclient);
}

CLadspaPlugin* CClientsManager::LoadPlugin(const std::string& source, JSONMap& client)
{
  JSONMap::iterator plugin = client.find("plugin");
  if (plugin == client.end())
  {
    LogError("%shas no plugin object", source.c_str());
    return NULL;
  }
  else if (!plugin->second->IsMap())
  {
    LogError("%sinvalid value for plugin %s", source.c_str(), ToJSON(plugin->second).c_str());
    return NULL;
  }

  JSONMap::iterator label = plugin->second->AsMap().find("label");
  if (label == plugin->second->AsMap().end())
  {
    LogError("%splugin object has no label", source.c_str());
    return NULL;
  }
  else if (!label->second->IsString())
  {
    LogError("%sinvalid value for plugin label %s", source.c_str(), ToJSON(label->second).c_str());
    return NULL;
  }

  JSONMap::iterator uniqueid = plugin->second->AsMap().find("uniqueid");
  if (uniqueid == plugin->second->AsMap().end())
  {
    LogError("%splugin object has no uniqueid", source.c_str());
    return NULL;
  }
  else if (!uniqueid->second->IsNumber())
  {
    LogError("%sinvalid value for plugin uniqueid %s", source.c_str(), ToJSON(uniqueid->second).c_str());
    return NULL;
  }

  CLadspaPlugin* ladspaplugin = m_bobdsp.PluginManager().GetPlugin(uniqueid->second->ToInt64(),
                                                                   label->second->AsString().c_str());
  if (ladspaplugin == NULL)
  {
    LogError("%sdid not find plugin with uniqueid %"PRIi64" and label \"%s\"",
             source.c_str(), uniqueid->second->ToInt64(), label->second->AsString().c_str());
    return NULL;
  }
  else
  {
    LogDebug("Found matching plugin for \"%s\" %"PRIi64" in %s", label->second->AsString().c_str(),
             uniqueid->second->ToInt64(), ladspaplugin->FileName().c_str());

    return ladspaplugin;
  }
}

bool CClientsManager::LoadControls(const std::string& source, JSONMap& client, std::vector<controlvalue>& controlvalues)
{
  JSONMap::iterator controls = client.find("controls");
  if (controls != client.end() && !controls->second->IsArray())
  {
    LogError("%sinvalid value for controls %s", source.c_str(), ToJSON(controls->second).c_str());
    return false;
  }

  //check if every control is valid
  for (JSONArray::iterator control = controls->second->AsArray().begin();
       control != controls->second->AsArray().end(); control++)
  {
    if (!(*control)->IsMap())
    {
      LogError("%sinvalid value for control %s", source.c_str(), ToJSON(*control).c_str());
      return false;
    }

    JSONMap::iterator name = (*control)->AsMap().find("name");
    if (name == (*control)->AsMap().end())
    {
      LogError("%scontrol has no name", source.c_str());
      return false;
    }
    else if (!name->second->IsString())
    {
      LogError("%sinvalid value for control name %s", source.c_str(), ToJSON(name->second).c_str());
      return false;
    }

    JSONMap::iterator value = (*control)->AsMap().find("value");
    if (value == (*control)->AsMap().end())
    {
      LogError("%scontrol \"%s\" has no value", source.c_str(), name->second->AsString().c_str());
      return false;
    }
    else if (!value->second->IsNumber())
    {
      LogError("%sinvalid value for control \"%s\": %s", source.c_str(),
               name->second->AsString().c_str(), ToJSON(value->second).c_str());
      return false;
    }

    //search back in the controls map, to check if this control has a duplicate
    for (vector<controlvalue>::iterator it = controlvalues.begin(); it != controlvalues.end(); it++)
    {
      if (it->first == name->second->AsString())
      {
        LogError("%scontrol \"%s\" has duplicate", source.c_str(), name->second->AsString().c_str());
        return false;
      }
    }

    //control is valid, store
    controlvalues.push_back(make_pair(name->second->AsString(), value->second->ToDouble()));
  }

  return true;
}

bool CClientsManager::CheckControls(const std::string& source, CLadspaPlugin* ladspaplugin, std::vector<controlvalue>& controlvalues)
{
  bool allcontrolsok = true;
  for (vector<controlvalue>::iterator it = controlvalues.begin(); it != controlvalues.end(); it++)
  {
    bool found = false;
    for (unsigned long port = 0; port < ladspaplugin->PortCount(); port++)
    {
      if (ladspaplugin->IsControlInput(port) && it->first == ladspaplugin->PortName(port))
      {
        LogDebug("Found control port \"%s\" in plugin \"%s\"", it->first.c_str(), ladspaplugin->Label());
        found = true;
        break;
      }
    }
    if (!found)
    {
      LogError("Did not find control port \"%s\" in plugin \"%s\"", it->first.c_str(), ladspaplugin->Label());
      allcontrolsok = false;
    }
  }

  //check if all control input ports are mapped
  for (unsigned long port = 0; port < ladspaplugin->PortCount(); port++)
  {
    if (ladspaplugin->IsControlInput(port))
    {
      bool found = false;
      for (vector<controlvalue>::iterator it = controlvalues.begin(); it != controlvalues.end(); it++)
      {
        if (it->first == ladspaplugin->PortName(port))
        {
          found = true;
          break;
        }
      }
      if (!found)
      {
        LogError("Control port \"%s\" of plugin \"%s\" is not mapped", ladspaplugin->PortName(port), ladspaplugin->Label());
        allcontrolsok = false;
      }
    }

    if (!ladspaplugin->PortDescriptorSanityCheck(port))
      allcontrolsok = false;
  }

  return allcontrolsok;
}

CJSONGenerator* CClientsManager::ClientsToJSON()
{
  CJSONGenerator* generator = new CJSONGenerator(true);

  generator->MapOpen();
  generator->AddString("clients");
  generator->ArrayOpen();

  CLock lock(m_mutex);

  for (vector<CJackClient*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
  {
    generator->MapOpen();

    generator->AddString("name");
    generator->AddString((*it)->Name());
    generator->AddString("instances");
    generator->AddInt((*it)->NrInstances());
    generator->AddString("pregain");
    generator->AddDouble((*it)->PreGain());
    generator->AddString("postgain");
    generator->AddDouble((*it)->PostGain());

    generator->AddString("controls");
    generator->ArrayOpen();
    const vector<controlvalue>& controls = (*it)->ControlInputs();
    for (vector<controlvalue>::const_iterator control = controls.begin(); control != controls.end(); control++)
    {
      generator->MapOpen();

      generator->AddString("name");
      generator->AddString(control->first);
      generator->AddString("value");
      generator->AddDouble(control->second);

      //add the port description of this port
      CLadspaPlugin* plugin = (*it)->Plugin();
      long port = plugin->PortByName(control->first);
      m_bobdsp.PluginManager().PortRangeDescriptionToJSON(*generator, plugin, port);

      generator->MapClose();
    }
    generator->ArrayClose();

    generator->AddString("plugin");
    generator->MapOpen();
    generator->AddString("label");
    generator->AddString((*it)->Plugin()->Label());
    generator->AddString("uniqueid");
    generator->AddInt((*it)->Plugin()->UniqueID());
    generator->MapClose();

    generator->MapClose();
  }

  generator->ArrayClose();
  generator->MapClose();

  return generator;
}

bool CClientsManager::Process(bool& triedconnect, bool& allconnected, int64_t lastconnect)
{
  CLock lock(m_mutex);

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

int CClientsManager::ClientPipes(pollfd*& fds, int extra)
{
  CLock lock(m_mutex);

  fds = new pollfd[m_clients.size() + extra];

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
    CLock lock(m_mutex);
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
    lock.Leave();

    //in case of a jack event, every connected client sends a message
    //to make sure we get them all in one go, if a message from one client is received
    //wait a millisecond, then check all clients for messages again
    if (gotmessage && i == 0)
      USleep(1000);
    else
      break;
  }
}

