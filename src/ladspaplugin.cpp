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

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>

#include "ladspaplugin.h"
#include "util/log.h"
#include "util/misc.h"

using namespace std;

CLadspaPlugin::CLadspaPlugin(const std::string& filename, void* handle, const LADSPA_Descriptor* descriptor)
{
  m_filename   = filename;
  m_handle     = handle;
  m_descriptor = descriptor;
}

CLadspaPlugin::~CLadspaPlugin()
{
  dlclose(m_handle);
}

void CLadspaPlugin::GetPlugins(std::string path, std::vector<CLadspaPlugin*>& plugins)
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
      Log("Unable to load %s: %s", entry->d_name, dlerror());
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
          plugins.push_back(new CLadspaPlugin(path + string(entry->d_name), handleprivate, descriptor));
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
      dlclose(handle);
    }
  }

  closedir(ladspadir);
}

void CLadspaPlugin::LoadAllSymbols()
{
  //open a new handle with all symbols loaded
  void* handle = dlopen(m_filename.c_str(), RTLD_NOW);
  if (handle)
  {
    dlclose(m_handle); //close the old handle
    m_handle = handle;
  }
  else
  {
    LogError("Unable to open new handle for %s: %s", m_filename.c_str(), dlerror());
  }
}

int CLadspaPlugin::AudioInputPorts()
{
  int ports = 0;
  for (unsigned long i = 0; i < m_descriptor->PortCount; i++)
  {
    if (LADSPA_IS_PORT_INPUT(m_descriptor->PortDescriptors[i]) &&
        LADSPA_IS_PORT_AUDIO(m_descriptor->PortDescriptors[i]))
      ports++;
  }

  return ports;
}

int CLadspaPlugin::AudioOutputPorts()
{
  int ports = 0;
  for (unsigned long i = 0; i < m_descriptor->PortCount; i++)
  {
    if (LADSPA_IS_PORT_OUTPUT(m_descriptor->PortDescriptors[i]) &&
        LADSPA_IS_PORT_AUDIO(m_descriptor->PortDescriptors[i]))
      ports++;
  }

  return ports;
}

const LADSPA_PortDescriptor CLadspaPlugin::PortDescriptor(unsigned long port)
{
  if (port < PortCount())
    return m_descriptor->PortDescriptors[port];
  else
    return 0;
}

const char* CLadspaPlugin::PortName(unsigned long port)
{
  if (port < PortCount())
    return m_descriptor->PortNames[port];
  else
    return NULL;
}

