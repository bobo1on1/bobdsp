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

#include <vector>
#include <string>
#include <utility>

#include "ladspaplugin.h"
#include "jackclient.h"
#include "portconnector.h"
#include "httpserver.h"
#include "clientmessage.h"
#include "pluginmanager.h"
#include "clientsmanager.h"
#include "visualizer.h"

class CBobDSP
{
  public:
    CBobDSP(int argc, char *argv[]);
    ~CBobDSP();

    void Setup();
    void Process();
    void Cleanup();

    CPortConnector&  PortConnector()   { return m_portconnector;  }
    CPluginManager&  PluginManager()   { return m_pluginmanager;  }
    CClientsManager& ClientsManager()  { return m_clientsmanager; }
    CVisualizer&     Visualizer()      { return m_visualizer;     }

  private:
    CPortConnector  m_portconnector;
    CPluginManager  m_pluginmanager;
    CClientsManager m_clientsmanager;
    CVisualizer     m_visualizer;
    CHttpServer     m_httpserver;
    bool            m_stop;
    int             m_signalfd;
    int             m_stdout[2];
    int             m_stderr[2];

    void PrintHelpMessage();
    void SetupRT(int64_t memsize);
    void SetupSignals();
    void RoutePipe(FILE*& file, int* pipe);
    void ProcessMessages(int64_t timeout);
    void ProcessClientMessages(pollfd* fds, int nrclientpipes);
    void ProcessSignalfd();
    void ProcessStdFd(const char* name, int& fd);
    void ProcessManagerMessages(CMessagePump& manager);

    void LoadLadspaPaths(std::vector<std::string>& ladspapaths);
    void LoadSettings();

    static void JackError(const char* jackerror);
    static void JackInfo(const char* jackinfo);
};

#endif //BOBDSP_H
