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

#include "pluginmanager.h"
#include "util/log.h"
#include "util/misc.h"
#include "util/lock.h"

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>

using namespace std;

CPluginManager::CPluginManager()
{
  m_samplerate = 48000; //safe default
}

CPluginManager::~CPluginManager()
{
  for (list<CLadspaPlugin*>::iterator it = m_plugins.begin(); it != m_plugins.end(); it++)
    delete *it;
}

void CPluginManager::LoadPlugins(std::vector<std::string>& paths)
{
  CLock lock(m_mutex);

  //load plugins from the paths
  for (vector<string>::iterator it = paths.begin(); it != paths.end(); it++)
    LoadPluginsPath(*it);

  //sort plugins by name
  m_plugins.sort(CLadspaPlugin::SortByName);

  Log("Found %zu plugins", m_plugins.size());

  if (g_printdebuglevel)
  {
    for (list<CLadspaPlugin*>::iterator it = m_plugins.begin(); it != m_plugins.end(); it++)
    {
      const LADSPA_Descriptor* d = (*it)->Descriptor();
      LogDebug("ID:%lu Label:\"%s\" Name:\"%s\" Ports:%lu", d->UniqueID, d->Label, d->Name, d->PortCount);
    }
  }
}

void CPluginManager::LoadPluginsPath(std::string& path)
{
  Log("Loading plugins from %s", path.c_str());

  DIR* ladspadir = opendir(path.c_str());
  if (!ladspadir)
  {
    LogError("Unable to open directory %s: %s", path.c_str(), GetErrno().c_str());
    return;
  }

  //iterate over the directory, try to open files with dlopen
  struct dirent* entry;
  for(;;)
  {
    errno = 0;
    entry = readdir(ladspadir);
    if (entry == NULL)
    {
      if (errno != 0) //maybe we should continue reading here?
        LogError("Unable to read directory %s: %s", path.c_str(), GetErrno().c_str());

      break; //done reading
    }

    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    void* handle = dlopen((string(path) + entry->d_name).c_str(), RTLD_LAZY);
    if (handle == NULL)
    {
      LogError("Unable to load %s: %s", entry->d_name, dlerror());
      continue;
    }

    LADSPA_Descriptor_Function ladspafunc = (LADSPA_Descriptor_Function)dlsym(handle, "ladspa_descriptor");
    if (ladspafunc)
    {
      const LADSPA_Descriptor* descriptor;
      int index;
      for(index = 0;;index++)
      {
        descriptor = ladspafunc(index);
        if (descriptor)
        {
          //open another handle, since CLadspaPlugin calls dlclose() in the destructor
          void* handleprivate = dlopen((string(path) + entry->d_name).c_str(), RTLD_LAZY);
          if (!handleprivate)
          {
            LogError("Unable to open private handle for %s: %s", entry->d_name, dlerror());
            continue;
          }
          CLock lock(m_mutex);
          m_plugins.push_back(new CLadspaPlugin(path + string(entry->d_name), handleprivate, descriptor));
        }
        else
        {
          break;
        }
      }
      LogDebug("Found %i plugin(s) in %s", index, entry->d_name);
    }
    else
    {
      LogError("Unable to get ladspa_descriptor from %s: %s", entry->d_name, dlerror());
    }

    dlclose(handle);
  }

  closedir(ladspadir);
}

CLadspaPlugin* CPluginManager::GetPlugin(int64_t uniqueid, const char* label)
{
  CLock lock(m_mutex);
  CLadspaPlugin* ladspaplugin = NULL;
  for (list<CLadspaPlugin*>::iterator it = m_plugins.begin(); it != m_plugins.end(); it++)
  {
    if (uniqueid == (int64_t)(*it)->UniqueID() && strcmp((*it)->Label(), label) == 0)
    {
      ladspaplugin = (*it);
      break;
    }
  }

  return ladspaplugin;
}

std::string CPluginManager::PluginsToJSON()
{
  CJSONGenerator generator;

  generator.MapOpen();
  generator.AddString("plugins");
  generator.ArrayOpen();

  CLock lock(m_mutex);
  for (list<CLadspaPlugin*>::iterator it = m_plugins.begin(); it != m_plugins.end(); it++)
  {
    generator.MapOpen();

    generator.AddString("name");
    generator.AddString((*it)->Name());
    generator.AddString("label");
    generator.AddString((*it)->Label());
    generator.AddString("maker");
    generator.AddString((*it)->Maker());
    generator.AddString("copyright");
    generator.AddString((*it)->Copyright());
    generator.AddString("uniqueid");
    generator.AddInt((*it)->UniqueID());
    generator.AddString("filename");
    generator.AddString((*it)->FileName());

    generator.AddString("ports");
    generator.ArrayOpen();
    for (unsigned long port = 0; port < (*it)->PortCount(); port++)
    {
      generator.MapOpen();
      generator.AddString("name");
      generator.AddString((*it)->PortName(port));
      generator.AddString("direction");
      generator.AddString((*it)->DirectionStr(port));
      generator.AddString("type");
      generator.AddString((*it)->TypeStr(port));
      generator.MapClose();
    }
    generator.ArrayClose();

    generator.AddString("controls");
    generator.ArrayOpen();
    for (unsigned long port = 0; port < (*it)->PortCount(); port++)
    {
      if ((*it)->IsControlInput(port))
      {
        generator.MapOpen();
        generator.AddString("name");
        generator.AddString((*it)->PortName(port));
        PortRangeDescriptionToJSON(generator, *it, port);
        generator.MapClose();
      }
    }
    generator.ArrayClose();

    generator.MapClose();
  }
  lock.Leave();

  generator.ArrayClose();
  generator.MapClose();

  return generator.ToString();
}

void CPluginManager::PortRangeDescriptionToJSON(CJSONGenerator& generator, CLadspaPlugin* plugin, unsigned long port)
{
  if (plugin->IsControlInput(port))
  {
    generator.AddString("toggled");
    generator.AddBool(plugin->IsToggled(port));
    generator.AddString("logarithmic");
    generator.AddBool(plugin->IsLogarithmic(port));
    generator.AddString("integer");
    generator.AddBool(plugin->IsInteger(port));

    CLock lock(m_mutex);

    generator.AddString("hasdefault");
    generator.AddBool(plugin->HasDefault(port));
    generator.AddString("default");
    generator.AddDouble(plugin->DefaultValue(port, m_samplerate));

    if (plugin->HasUpperBound(port))
    {
      generator.AddString("upperbound");
      generator.AddDouble(plugin->UpperBound(port, m_samplerate));
    }
    if (plugin->HasLowerBound(port))
    {
      generator.AddString("lowerbound");
      generator.AddDouble(plugin->LowerBound(port, m_samplerate));
    }
  }
}

void CPluginManager::SetSamplerate(int samplerate)
{
  if (m_samplerate != samplerate)
  {
    Log("Samplerate updated from %i to %i", m_samplerate, samplerate);

    CLock lock(m_mutex);
    m_samplerate = samplerate;
  }
}

