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

#ifndef PORTCONNECTOR_H
#define PORTCONNECTOR_H

#include <utility>
#include <string>
#include <vector>
#include <jack/jack.h>

struct portconnection
{
  std::string in;
  std::string out;
  bool indisconnect;
  bool outdisconnect;
};

class CPortConnector
{
  public:
    CPortConnector();
    ~CPortConnector();

    void AddConnection(portconnection& connection);

    bool Connect();
    void Disconnect();

    void Process(bool& portregistered, bool& portconnected);

  private:
    std::vector<portconnection> m_connections;

    jack_client_t* m_client;
    bool           m_connected;

    bool ConnectInternal();
    void ProcessInternal(bool& portregistered, bool& portconnected);

    void ConnectPorts(const char** ports);
    void DisconnectPorts(const char** ports);
};

#endif //PORTCONNECTOR_H
