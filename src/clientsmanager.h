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

#ifndef CLIENTSMANAGER_H
#define CLIENTSMANAGER_H

#include "util/inclstdint.h"

#include <vector>
#include <string>
#include <poll.h>

#include "jackclient.h"
#include "util/incltinyxml.h"
#include "util/mutex.h"

class CBobDSP;

class CClientsManager
{
  public:
    CClientsManager(CBobDSP& bobdsp);
    ~CClientsManager();

    void        Stop();

    void        ClientsFromXML(TiXmlElement* root);
    bool        ClientsFromJSON(const std::string& json);
    std::string ClientsToJSON();

    bool        Process(bool& triedconnect, bool& allconnected, int64_t lastconnect);

    int         NrClients() { return m_clients.size(); }
    int         ClientPipes(pollfd* fds);

    void        ProcessMessages(bool& checkconnect, bool& checkdisconnect, bool& updateports);

  private:
    CBobDSP&                  m_bobdsp;
    std::vector<CJackClient*> m_clients;
    CMutex                    m_mutex;

    bool LoadControlsFromClient(TiXmlElement* client, std::vector<controlvalue>& controlvalues);
};

#endif //CLIENTSMANAGER_H
