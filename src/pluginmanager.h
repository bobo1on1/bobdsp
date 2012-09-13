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

#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include <list>
#include <string>
#include <vector>
#include "ladspaplugin.h"
#include "util/inclstdint.h"
#include "util/mutex.h"

class CPluginManager
{
  public:
    CPluginManager();
    ~CPluginManager();

    void LoadPlugins(std::vector<std::string>& paths);
    CLadspaPlugin* GetPlugin(int64_t uniqueid, const char* label);
    std::string PluginsToJSON();
    void SetSamplerate(int samplerate);

  private:
    void LoadPluginsPath(std::string& path);

    int                       m_samplerate;
    std::list<CLadspaPlugin*> m_plugins;
    CMutex                    m_mutex;
};

#endif //PLUGINMANAGER_H
