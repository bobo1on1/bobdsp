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
#include <fstream>
#include <assert.h>

using namespace std;

#define SETTINGSFILE ".bobdsp/clients.json"

CClientsManager::CClientsManager(CBobDSP& bobdsp):
  CMessagePump("clientsmanager"),
  CJSONSettings(SETTINGSFILE, "client", m_condition),
  m_bobdsp(bobdsp)
{
  m_stop = false;
  m_checkclients = false;
  m_clientindex = 0;
  m_controlindex = 0;
}

CClientsManager::~CClientsManager()
{
}

void CClientsManager::Stop()
{
  CLock lock(m_condition);

  Log("Stopping %zu jack client(s)", m_clients.size());
  for (vector<CJackLadspa*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
    delete *it;

  m_clients.clear();

  m_stop = true;
  m_condition.Broadcast();
}

CJSONGenerator* CClientsManager::SettingsToJSON(bool tofile)
{
  //when these settings are being written to a file, don't add the portdescription
  return ClientsToJSON(tofile);
}

void CClientsManager::LoadSettings(JSONMap& root, bool reload, bool allowreload, const std::string& source)
{
  if (reload)
  {
    //reload requested, mark all clients for delete, reload the file
    //and set the flag that clients are deleted
    if (!m_clients.empty())
    {
      Log("Reload requested, marking all existing clients for deletion");
      m_checkclients = true;
      for (vector<CJackLadspa*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
        (*it)->MarkDelete();
    }
  }

  //check the JSON elements first
  JSONMap::iterator clients = root.find("clients");
  if (clients != root.end() && !clients->second->IsArray())
  {
    LogError("%s: invalid value for clients: %s", source.c_str(), ToJSON(clients->second).c_str());
    return;
  }

  JSONMap::iterator action = root.find("action");
  if (action != root.end() && !action->second->IsString())
  {
    LogError("%s: invalid value for action: %s", source.c_str(), ToJSON(action->second).c_str());
    return;
  }

  JSONMap::iterator timeout = root.find("timeout");
  if (timeout != root.end() && !timeout->second->IsNumber())
  {
    LogError("%s: invalid value for timeout: %s", source.c_str(), ToJSON(timeout->second).c_str());
    return;
  }

  JSONMap::iterator clientindex = root.find("clientindex");
  if (clientindex != root.end() && !clientindex->second->IsNumber())
  {
    LogError("%s: invalid value for clientindex: %s", source.c_str(), ToJSON(clientindex->second).c_str());
    return;
  }

  JSONMap::iterator controlindex = root.find("controlindex");
  if (controlindex != root.end() && !controlindex->second->IsNumber())
  {
    LogError("%s: invalid value for controlindex: %s", source.c_str(), ToJSON(controlindex->second).c_str());
    return;
  }

  //if all the JSON elements are valid, parse them
  if (clients != root.end())
  {
    for (JSONArray::iterator it = clients->second->AsArray().begin(); it != clients->second->AsArray().end(); it++)
      LoadClient(*it, source + ": ");
  }

  if (action != root.end())
  {
    if (action->second->AsString() == "save")
      SaveFile();
    else if (action->second->AsString() == "reload" && allowreload)
      LoadFile(true);
    else if (action->second->AsString() == "wait")
      WaitForChange(root, timeout, clientindex, controlindex);
  }

  //check if a message need to be sent to the main thread
  if (m_checkclients)
  {
    WriteSingleMessage(MsgCheckClients);
    m_checkclients = false;
  }
}

void CClientsManager::LoadClient(CJSONElement* jsonclient, std::string source)
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
    return; //the client name is used as identifier, it needs to be unique
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
  for (int i = 0; i < 2; i++)
  {
    if (LoadDouble(client, gain[i], i == 0 ? "pregain" : "postgain", source) == INVALID)
      return; //gain invalid
  }

  CLadspaPlugin* ladspaplugin = LoadPlugin(source, client);
  if (ladspaplugin == NULL)
    return; //plugin not set, not found or invalid

  controlmap controlvalues;
  if (!LoadControls(source, client, controlvalues))
    return; //control values invalid

  if (!CheckControls(source, ladspaplugin, controlvalues, true))
    return; //control values don't match the plugin

  //everything ok, allocate a new client
  CJackLadspa* jackclient = new CJackLadspa(ladspaplugin, name, instances,
                                            gain, controlvalues);
  m_clients.push_back(jackclient);
  m_checkclients = true;

  m_clientindex++;
  m_condition.Broadcast();

  Log("Added client \"%s\" instances:%"PRIi64" pregain:%.3f postgain:%.3f",
      name.c_str(), instances, gain[0], gain[1]);
}

void CClientsManager::DeleteClient(JSONMap& client, const std::string& name, const std::string& source)
{
  CJackLadspa* jackclient = FindClient(name);
  if (jackclient == NULL)
  {
    LogError("%sdoesn't exist", source.c_str());
    return;
  }

  LogDebug("Marking client \"%s\" for delete", name.c_str());
  //mark the client for deletion
  //clients are deleted from the main thread, since it's waiting on its messagepipe
  jackclient->MarkDelete(); 
  m_checkclients = true;

  m_clientindex++;
  m_condition.Broadcast();
}

void CClientsManager::UpdateClient(JSONMap& client, const std::string& name, const std::string& source)
{
  CJackLadspa* jackclient = FindClient(name);
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
    return; //invalid control values

  if (!CheckControls(source, jackclient->Plugin(), controlvalues, false))
    return; //one or more control values don't exist in the plugin

  bool controlupdated = false;

  if (instancesupdated && instances != jackclient->NrInstances())
  {
    //apply new instances, then tell the main thread to restart this client
    Log("Client \"%s\" setting instances to %"PRIi64, name.c_str(), instances);
    jackclient->SetNrInstances(instances);
    jackclient->MarkRestart();
    m_checkclients = true;
    controlupdated = true;
  }

  //update gain values
  for (int i = 0; i < 2; i++)
  {
    if (gainupdated[i])
    {
      LogDebug("Client \"%s\" setting %s to %f", name.c_str(), i == 0 ? "pregain" : "postgain", gain[i]);
      jackclient->UpdateGain(gain[i], i);
      controlupdated = true;
    }
  }

  //update control values
  if (!controlvalues.empty())
  {
    jackclient->UpdateControls(controlvalues);
    controlupdated = true;
  }

  if (controlupdated)
  {
    m_controlindex++;
    m_condition.Broadcast();
  }
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

  JSONMap::iterator filename = plugin->second->AsMap().find("filename");
  if (filename == plugin->second->AsMap().end())
  {
    LogError("%splugin object has no filename", source.c_str());
    return NULL;
  }
  else if (!filename->second->IsString())
  {
    LogError("%sinvalid value for plugin filename %s", source.c_str(), ToJSON(filename->second).c_str());
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
                                                                   label->second->AsString(),
                                                                   filename->second->AsString());
  if (ladspaplugin == NULL)
  {
    LogError("%sdid not find plugin %s %"PRIi64" in %s", source.c_str(),
             label->second->AsString().c_str(), uniqueid->second->ToInt64(),
             filename->second->AsString().c_str());
  }
  else
  {
    LogDebug("Found plugin %s %"PRIi64" in %s", label->second->AsString().c_str(),
             uniqueid->second->ToInt64(), ladspaplugin->FileName().c_str());
  }

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

    LogDebug("Loaded control \"%s\" with value %f",
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

CJackLadspa* CClientsManager::FindClient(const std::string& name)
{
  for (vector<CJackLadspa*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
  {
    if ((*it)->NeedsDelete())
      continue; //don't consider clients that have been marked for delete

    if ((*it)->Name() == name)
      return *it;
  }

  return NULL;
}

//should have locked m_condition before calling this
void CClientsManager::WaitForChange(JSONMap& root, JSONMap::iterator& timeout,
                                    JSONMap::iterator& clientindex, JSONMap::iterator& controlindex)
{
  int64_t ptimeout = -1;
  if (timeout != root.end())
    ptimeout = Min(timeout->second->ToInt64(), 60000) * 1000;

  int64_t pclientindex = -1;
  if (clientindex != root.end())
    pclientindex = clientindex->second->ToInt64();

  int64_t pcontrolindex = -1;
  if (controlindex != root.end())
    pcontrolindex = controlindex->second->ToInt64();

  //wait on m_condtion for changes on m_clientindex, m_controlindex or the timeout
  int64_t start = GetTimeUs();
  int64_t now   = start;
  while (!m_stop && (pclientindex == -1 || (int64_t)m_clientindex == pclientindex) &&
         (pcontrolindex == -1 || (int64_t)m_controlindex == pcontrolindex) && (now - start) < ptimeout)
  {
    m_condition.Wait(Max(ptimeout - (now - start), 0));
    now = GetTimeUs();
  }
}

CJSONGenerator* CClientsManager::ClientsToJSON(bool tofile)
{
  CJSONGenerator* generator = new CJSONGenerator(true);

  generator->MapOpen();

  CLock lock(m_condition);

  if (!tofile)
  {
    generator->AddString("clientindex");
    generator->AddInt(m_clientindex);
    generator->AddString("controlindex");
    generator->AddInt(m_controlindex);
  }

  generator->AddString("clients");
  generator->ArrayOpen();

  for (vector<CJackLadspa*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
  {
    if ((*it)->NeedsDelete())
      continue; //don't add clients that have been marked for delete

    generator->MapOpen();

    generator->AddString("name");
    generator->AddString((*it)->Name());
    generator->AddString("instances");
    generator->AddInt((*it)->NrInstances());

    generator->AddString("pregain");
    generator->AddDouble((*it)->GetGain(0));
    generator->AddString("postgain");
    generator->AddDouble((*it)->GetGain(1));

    generator->AddString("controls");
    generator->ArrayOpen();

    controlmap controls;
    (*it)->GetControlInputs(controls);

    //iterate over the ladspa plugin's control inputs,
    //then find the corresponding control in the jack client's controls
    //this way, the order of the controls in the JSON output
    //is the same as the controls order of the ladspa plugin
    CLadspaPlugin* plugin = (*it)->Plugin();
    for (unsigned long port = 0; port < plugin->PortCount(); port++)
    {
      if (plugin->IsControlInput(port))
      {
        controlmap::iterator control = controls.find(plugin->PortName(port));
        assert(control != controls.end());

        generator->MapOpen();

        generator->AddString("name");
        generator->AddString(control->first);
        generator->AddString("value");
        generator->AddDouble(control->second);

        //add the port description of this port
        if (!tofile)
          m_bobdsp.PluginManager().PortRangeDescriptionToJSON(*generator, plugin, port);

        generator->MapClose();
      }
    }

    generator->ArrayClose();

    generator->AddString("plugin");
    generator->MapOpen();
    generator->AddString("label");
    generator->AddString((*it)->Plugin()->Label());
    generator->AddString("uniqueid");
    generator->AddInt((*it)->Plugin()->UniqueID());
    generator->AddString("filename");
    generator->AddString((*it)->Plugin()->FileName());
    generator->MapClose();

    generator->MapClose();
  }

  generator->ArrayClose();
  generator->MapClose();

  return generator;
}

void CClientsManager::Process(bool& triedconnect, bool& allconnected, bool tryconnect)
{
  CLock lock(m_condition);

  //check if any client has exited and needs to be disconnected
  for (vector<CJackLadspa*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
    (*it)->CheckExitStatus();

  vector<CJackLadspa*>::iterator it = m_clients.begin();
  while (it != m_clients.end())
  {
    //if this client has been marked for removal, delete it and remove it from the vector
    if ((*it)->NeedsDelete())
    {
      Log("Deleting client \"%s\"", (*it)->Name().c_str());
      delete *it;
      it = m_clients.erase(it);
      continue;
    }

    //disconnect the client if it needs a restart, it'll get reconnected below
    if ((*it)->NeedsRestart())
    {
      if ((*it)->IsConnected())
      {
        Log("Disconnecting client \"%s\" because of restart", (*it)->Name().c_str());
        (*it)->Disconnect();
      }

      (*it)->ClearRestart();
    }

    //keep trying to connect
    //only try to connect at the interval to prevent hammering jackd
    if (!(*it)->IsConnected())
    {
      if (tryconnect)
      {
        triedconnect = true;
        if ((*it)->Connect())
        {
          //update samplerate
          if ((*it)->Samplerate() != 0)
            m_bobdsp.PluginManager().SetSamplerate((*it)->Samplerate());
        }
      }
    }

    //if not all clients are connected, the main thread needs to retry
    if (!(*it)->IsConnected())
      allconnected = false;

    it++;
  }
}

int CClientsManager::ClientPipes(pollfd*& fds, int extra)
{
  CLock lock(m_condition);

  fds = new pollfd[m_clients.size() + extra];

  int nrfds = 0;
  for (vector<CJackLadspa*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
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

void CClientsManager::ProcessMessages()
{
  CLock lock(m_condition);
  for (vector<CJackLadspa*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
  {
    ClientMessage msg;
    while ((msg = (*it)->GetMessage()) != MsgNone)
    {
      LogDebug("got message %s from client \"%s\"", MsgToString(msg), (*it)->Name().c_str());
      (*it)->ConfirmMessage(msg);
    }
  }
}

