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

#ifndef BOBDSP_H
#define BOBDSP_H

#include "util/inclstdint.h"
#include "util/incltinyxml.h"

#include <vector>
#include <string>
#include <utility>

#include "ladspaplugin.h"
#include "jackclient.h"
#include "portconnector.h"

class CBobDSP
{
  public:
    CBobDSP(int argc, char *argv[]);
    ~CBobDSP();

    void Process();

  private:

    bool                        m_stop;
    std::vector<CLadspaPlugin*> m_plugins;
    std::vector<CJackClient*>   m_clients;
    CPortConnector              m_portconnector;
    int                         m_signalfd;

    void SetupRT(int64_t memsize);
    void SetupSignals();
    void ProcessMessages(bool& portregistered, bool& portconnected, bool usetimeout,
                         std::vector< std::pair<int, CJackClient*> > pipes);
    void ProcessSignalfd();

    void LoadLadspaPaths(std::vector<std::string>& ladspapaths);
    bool LoadPluginsFromFile();
    void LoadPluginsFromRoot(TiXmlElement* root);
    bool LoadPortsFromPlugin(TiXmlElement* plugin, std::vector<portvalue>& portvalues);
    CLadspaPlugin* SearchLadspaPlugin(std::vector<CLadspaPlugin*> plugins, int64_t uniqueid, const char* label);
    bool PortDescriptorSanityCheck(CLadspaPlugin* plugin, unsigned long port, LADSPA_PortDescriptor p);
    bool LoadConnectionsFromFile();

};

#endif //BOBDSP_H
