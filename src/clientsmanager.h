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

#include <string>
#include <vector>
#include <poll.h>

#include "jackladspa.h"
#include "ladspaplugin.h"
#include "clientmessage.h"
#include "jsonsettings.h"
#include "util/condition.h"
#include "util/JSON.h"

class CBobDSP;

class CClientsManager : public CMessagePump, public CJSONSettings
{
  public:
    CClientsManager(CBobDSP& bobdsp);
    ~CClientsManager();

    void            Stop();
    void            Process(bool& triedconnect, bool& allconnected, bool tryconnect);
    int             ClientPipes(pollfd*& fds, int extra);
    void            ProcessMessages();

    CJSONGenerator* ClientsToJSON(bool tofile);

  private:
    CBobDSP&                  m_bobdsp;
    std::vector<CJackLadspa*> m_clients;
    CCondition                m_condition;
    bool                      m_checkclients;
    bool                      m_stop;
    int64_t                   m_clientindex;  //changed whenever a client is added or deleted
    int64_t                   m_controlindex; //changed whenever a control is changed
    int64_t                   m_fileindex;    //set to m_clientindex whenever settings are loaded from a file

    enum LOADSTATE
    {
      NOTFOUND,
      INVALID,
      SUCCESS
    };

    virtual CJSONGenerator* SettingsToJSON(bool tofile);
    virtual void            LoadSettings(JSONMap& root, bool reload, bool fromfile, const std::string& source);
    void                    LoadClient(CJSONElement* jsonclient, bool update, std::string source);
    void                    AddClient(JSONMap& client, const std::string& name, const std::string& source);
    void                    DeleteClient(JSONMap& client, const std::string& name, const std::string& source);
    void                    UpdateClient(JSONMap& client, const std::string& name, const std::string& source);
    LOADSTATE               LoadDouble(JSONMap& client, double& value, const std::string& name, const std::string& source);
    LOADSTATE               LoadInt64(JSONMap& client, int64_t& value, const std::string& name, const std::string& source);
    CLadspaPlugin*          LoadPlugin(const std::string& source, JSONMap& client);
    bool                    LoadControls(const std::string& source, JSONMap& client, controlmap& controlvalues);
    bool                    CheckControls(const std::string& source, CLadspaPlugin* ladspaplugin,
                                          controlmap& controlvalues, bool checkmissing);
    CJackLadspa*            FindClient(const std::string& name);
    void                    WaitForChange(JSONMap& root, JSONMap::iterator& timeout,
                                          JSONMap::iterator& clientindex, JSONMap::iterator& controlindex,
                                          JSONMap::iterator& uuid);
};

#endif //CLIENTSMANAGER_H
