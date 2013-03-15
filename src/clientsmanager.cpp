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
  CMessagePump("clientsmanager"),
  m_bobdsp(bobdsp)
{
  ResetFlags();
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
  ResetFlags();
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

  CheckFlags();

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

  LogDebug("Parsing settings for client \"%s\"", name->second->AsString().c_str());

  //print the client name in the source
  source += "client \"";
  source += name->second->AsString();
  source += "\" ";

  //the action decides what will be done with this JSON client object
  //if no action is given, then the default is to add a new client
  JSONMap::iterator action = client.find("action");
  if (action != client.end() && !action->second->IsString())
    LogError("%sinvalid value for action: %s", source.c_str(), ToJSON(action->second).c_str());
  else if (action == client.end() || action->second->AsString() == "add")
    AddClient(client, name->second->AsString(), source);
  else if (action->second->AsString() == "delete")
    DeleteClient(client, name->second->AsString(), source);
  else if (action->second->AsString() == "update")
    UpdateClient(client, name->second->AsString(), source);
  else
    LogError("%sinvalid action: \"%s\"", source.c_str(), action->second->AsString().c_str());
}

void CClientsManager::AddClient(JSONMap& client, const std::string& name, const std::string& source)
{
  if (FindClient(name) != NULL)
  {
    LogError("%salready exists", source.c_str());
    return;
  }

  int64_t instances = 1;
  if (LoadInt64(client, instances, "instances", source) == INVALID)
    return;

  if (instances < 1)
  {
    LogError("%sinvalid value for instances: %"PRIi64, source.c_str(), instances);
    return;
  }

  double gain[2] = {1.0, 1.0};
  if (LoadDouble(client, gain[0], "pregain", source) == INVALID)
    return;

  if (LoadDouble(client, gain[1], "postgain", source) == INVALID)
    return;

  CLadspaPlugin* ladspaplugin = LoadPlugin(source, client);
  if (ladspaplugin == NULL)
    return;

  controlmap controlvalues;
  if (!LoadControls(source, client, controlvalues))
    return;

  if (!CheckControls(source, ladspaplugin, controlvalues, true))
    return;

  CJackClient* jackclient = new CJackClient(ladspaplugin, name, instances,
                                            gain, controlvalues);
  m_clients.push_back(jackclient);
  m_clientadded = true;

  Log("Added client \"%s\" instances:%"PRIi64" pregain:%.3f postgain:%.3f",
      name.c_str(), instances, gain[0], gain[1]);
}

void CClientsManager::DeleteClient(JSONMap& client, const std::string& name, const std::string& source)
{
  CJackClient* jackclient = FindClient(name);
  if (jackclient == NULL)
  {
    LogError("%sdoesn't exist", source.c_str());
    return;
  }

  LogDebug("Marking client \"%s\" for delete", name.c_str());
  jackclient->MarkDelete();
  m_clientdeleted = true;
}

void CClientsManager::UpdateClient(JSONMap& client, const std::string& name, const std::string& source)
{
  CJackClient* jackclient = FindClient(name);
  if (jackclient == NULL)
  {
    LogError("%sdoesn't exist", source.c_str());
    return;
  }

  //load instances, gain and control values first, and apply them only after they've
  //all been checked and are all valid
  bool    instancesupdated = false;
  int64_t instances;
  LOADSTATE state = LoadInt64(client, instances, "instances", source);
  if (state == SUCCESS)
  {
    if (instances < 1)
    {
      LogError("%sinvalid value for instances: %"PRIi64, source.c_str(), instances);
      return;
    }
    instancesupdated = true;
  }
  else if (state == INVALID)
  {
    return;
  }

  bool   gainupdated[2] = { false, false };
  double gain[2];

  for (int i = 0; i < 2; i++)
  {
    state = LoadDouble(client, gain[i], i == 0 ? "pregain" : "postgain", source);
    if (state == SUCCESS)
      gainupdated[i] = true;
    else if (state == INVALID)
      return;
  }

  controlmap controlvalues;
  if (!LoadControls(source, client, controlvalues))
    return;

  if (!CheckControls(source, jackclient->Plugin(), controlvalues, false))
    return;

  if (instancesupdated && instances != jackclient->NrInstances())
  {
    Log("%ssetting instances to %"PRIi64, source.c_str(), instances);
    jackclient->SetNrInstances(instances);
    jackclient->MarkRestart();
    m_clientupdated = true;
  }

  for (int i = 0; i < 2; i++)
  {
    if (gainupdated[i])
    {
      LogDebug("%ssetting %s to %f", source.c_str(), i == 0 ? "pregain" : "postgain", gain[i]);
      jackclient->UpdateGain(gain[i], i);
    }
  }

  if (!controlvalues.empty())
    jackclient->UpdateControls(controlvalues);
}

CClientsManager::LOADSTATE CClientsManager::LoadDouble(JSONMap& client, double& value,
                                                       const std::string& name, const std::string& source)
{
  JSONMap::iterator it = client.find(name);
  if (it == client.end())
  {
    return NOTFOUND;
  }
  else if (!it->second->IsNumber())
  {
    LogError("%sinvalid value for %s: %s", source.c_str(), name.c_str(), ToJSON(it->second).c_str());
    return INVALID;
  }
  else
  {
    value = it->second->ToDouble();
    return SUCCESS;
  }
}

CClientsManager::LOADSTATE CClientsManager::LoadInt64(JSONMap& client, int64_t& value,
                                                      const std::string& name, const std::string& source)
{
  JSONMap::iterator it = client.find(name);
  if (it == client.end())
  {
    return NOTFOUND;
  }
  else if (!it->second->IsNumber())
  {
    LogError("%sinvalid value for %s: %s", source.c_str(), name.c_str(), ToJSON(it->second).c_str());
    return INVALID;
  }
  else
  {
    value = it->second->ToInt64();
    return SUCCESS;
  }
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
    LogError("%sdid not find plugin with uniqueid %"PRIi64" and label \"%s\"",
             source.c_str(), uniqueid->second->ToInt64(), label->second->AsString().c_str());
  else
    LogDebug("Found matching plugin for \"%s\" %"PRIi64" in %s", label->second->AsString().c_str(),
             uniqueid->second->ToInt64(), ladspaplugin->FileName().c_str());

  return ladspaplugin;
}

bool CClientsManager::LoadControls(const std::string& source, JSONMap& client, controlmap& controlvalues)
{
  JSONMap::iterator controls = client.find("controls");
  if (controls != client.end() && !controls->second->IsArray())
  {
    LogError("%sinvalid value for controls %s", source.c_str(), ToJSON(controls->second).c_str());
    return false;
  }
  else if (controls == client.end())
  {
    return true;
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
    if (controlvalues.find(name->second->AsString()) != controlvalues.end())
    {
      LogError("%scontrol \"%s\" has duplicate", source.c_str(), name->second->AsString().c_str());
      return false;
    }

    LogDebug("%sloaded control \"%s\" with value %f", source.c_str(),
             name->second->AsString().c_str(), value->second->ToDouble());

    //control is valid, store
    controlvalues[name->second->AsString()] = value->second->ToDouble();
  }

  return true;
}

bool CClientsManager::CheckControls(const std::string& source, CLadspaPlugin* ladspaplugin,
                                    controlmap& controlvalues, bool checkmissing)
{
  bool allcontrolsok = true;
  //check if all controls exist in the ladspa plugin
  for (controlmap::iterator it = controlvalues.begin(); it != controlvalues.end(); it++)
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

  //check if all control input ports of the ladspa plugin are mapped
  if (checkmissing)
  {
    for (unsigned long port = 0; port < ladspaplugin->PortCount(); port++)
    {
      if (ladspaplugin->IsControlInput(port))
      {
        if (controlvalues.find(ladspaplugin->PortName(port)) == controlvalues.end())
        {
          LogError("Control port \"%s\" of plugin \"%s\" is not mapped", ladspaplugin->PortName(port), ladspaplugin->Label());
          allcontrolsok = false;
        }
      }
    }
  }

  return allcontrolsok;
}

CJackClient* CClientsManager::FindClient(const std::string& name)
{
  for (vector<CJackClient*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
  {
    if ((*it)->NeedsDelete())
      continue; //don't consider clients that have been marked for delete

    if ((*it)->Name() == name)
      return *it;
  }

  return NULL;
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
    if ((*it)->NeedsDelete())
      continue; //don't add clients that have been marked for delete

    generator->MapOpen();

    generator->AddString("name");
    generator->AddString((*it)->Name());
    generator->AddString("instances");
    generator->AddInt((*it)->NrInstances());

    generator->AddString("pregain");
    generator->AddDouble((*it)->GetGain()[0]);
    generator->AddString("postgain");
    generator->AddDouble((*it)->GetGain()[1]);

    generator->AddString("controls");
    generator->ArrayOpen();

    controlmap controls;
    (*it)->GetControlInputs(controls);

    for (controlmap::iterator control = controls.begin(); control != controls.end(); control++)
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
    //if this client has been marked for removal, delete it and remove it from the vector
    if ((*it)->NeedsDelete())
    {
      Log("Deleting client \"%s\"", (*it)->Name().c_str());
      delete *it;
      it = m_clients.erase(it);
      if (it == m_clients.end())
        break;
    }

    //disconnect the client if it needs a restart, it'll get reconnected below
    if((*it)->NeedsRestart())
    {
      Log("Disconnecting client \"%s\" because of restart", (*it)->Name().c_str());
      (*it)->Disconnect();
    }

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

void CClientsManager::ResetFlags()
{
  m_clientadded   = false;
  m_clientdeleted = false;
  m_clientupdated = false;
}

void CClientsManager::CheckFlags()
{
  if (m_clientadded)
    m_clientadded = !WriteMessage(MsgClientAdded);
    
  if (m_clientdeleted)
    m_clientdeleted = !WriteMessage(MsgClientDeleted);

  if (m_clientupdated)
    m_clientupdated = !WriteMessage(MsgClientUpdated);
}

