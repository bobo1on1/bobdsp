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
#include "httpserver.h"
#include "clientmessage.h"

class CBobDSP
{
  public:
    CBobDSP(int argc, char *argv[]);
    ~CBobDSP();

    void Setup();
    void Process();
    void Cleanup();

    CPortConnector& PortConnector() { return m_portconnector; }

    bool SaveConnectionsToFile(TiXmlElement* connections);
    bool LoadConnectionsFromFile();

  private:
    bool                        m_stop;
    std::vector<CLadspaPlugin*> m_plugins;
    std::vector<CJackClient*>   m_clients;
    CPortConnector              m_portconnector;
    bool                        m_checkconnect;
    bool                        m_checkdisconnect;
    bool                        m_updateports;
    int                         m_signalfd;
    int                         m_stdout[2];
    int                         m_stderr[2];
    CHttpServer                 m_httpserver;

    void SetupRT(int64_t memsize);
    void SetupSignals();
    void RoutePipe(FILE*& file, int* pipe);
    void ProcessMessages(bool usetimeout);
    void ProcessClientMessages();
    void ProcessSignalfd();
    void ProcessStdFd(const char* name, int& fd);
    void ProcessHttpServerMessages();

    void LoadLadspaPaths(std::vector<std::string>& ladspapaths);
    bool LoadPluginsFromFile();
    void LoadPluginsFromRoot(TiXmlElement* root);
    bool LoadPortsFromPlugin(TiXmlElement* plugin, std::vector<portvalue>& portvalues);
    CLadspaPlugin* SearchLadspaPlugin(std::vector<CLadspaPlugin*> plugins, int64_t uniqueid, const char* label);
    bool PortDescriptorSanityCheck(CLadspaPlugin* plugin, unsigned long port, LADSPA_PortDescriptor p);

    static void JackError(const char* jackerror);
    static void JackInfo(const char* jackinfo);
};

#endif //BOBDSP_H
