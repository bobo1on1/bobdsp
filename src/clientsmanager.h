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
#include "ladspaplugin.h"
#include "clientmessage.h"
#include "util/mutex.h"
#include "util/JSON.h"

class CBobDSP;

class CClientsManager : public CMessagePump
{
  public:
    CClientsManager(CBobDSP& bobdsp);
    ~CClientsManager();

    void            Stop();

    void            LoadSettingsFromFile(const std::string& filename);
    CJSONGenerator* LoadSettingsFromString(const std::string& strjson, const std::string& source, bool returnsettings = false);
    CJSONGenerator* ClientsToJSON();

    bool            Process(bool& triedconnect, bool& allconnected, int64_t lastconnect);

    int             ClientPipes(pollfd*& fds, int extra);

    void            ProcessMessages(bool& checkconnect, bool& checkdisconnect, bool& updateports);

  private:
    CBobDSP&                  m_bobdsp;
    std::vector<CJackClient*> m_clients;
    CMutex                    m_mutex;
    bool                      m_clientadded;
    bool                      m_clientdeleted;
    bool                      m_clientupdated;

    enum LOADSTATE
    {
      NOTFOUND,
      INVALID,
      SUCCESS
    };

    void            LoadSettings(CJSONElement* json, const std::string& source);
    void            LoadClientSettings(CJSONElement* jsonclient, std::string source);
    void            AddClient(JSONMap& client, const std::string& name, const std::string& source);
    void            DeleteClient(JSONMap& client, const std::string& name, const std::string& source);
    LOADSTATE       LoadDouble(JSONMap& client, double& value, const std::string& name, const std::string& source);
    LOADSTATE       LoadInt64(JSONMap& client, int64_t& value, const std::string& name, const std::string& source);
    CLadspaPlugin*  LoadPlugin(const std::string& source, JSONMap& client);
    bool            LoadControls(const std::string& source, JSONMap& client, std::vector<controlvalue>& controlvalues);
    bool            CheckControls(const std::string& source, CLadspaPlugin* ladspaplugin, std::vector<controlvalue>& controlvalues);
    CJackClient*    FindClient(const std::string& name);

    void            ResetFlags();
    void            CheckFlags();
};

#endif //CLIENTSMANAGER_H
