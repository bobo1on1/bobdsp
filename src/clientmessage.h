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

#ifndef CLIENTMESSAGE_H
#define CLIENTMESSAGE_H

#include "util/inclstdint.h"

enum ClientMessage
{
  MsgNone,
  MsgExited,
  MsgPortRegistered,
  MsgPortDeregistered,
  MsgPortConnected,
  MsgPortDisconnected,
  MsgSamplerateChanged,
  MsgConnectionsUpdated,
  MsgCheckClients,
};

const char* MsgToString(ClientMessage msg);

inline const char* MsgToString(uint8_t msg)
{
  return MsgToString((ClientMessage)msg);
}

class CMessagePump
{
  public:
    CMessagePump(const char* name);
    ~CMessagePump();

    int           MsgPipe() { return m_pipe[0]; }
    ClientMessage GetMessage();
    bool          WriteMessage(uint8_t msg);
    const char*   Name() { return m_name; }

  private:
    const char* m_name;
    int         m_pipe[2];
};

#endif //CLIENTMESSAGE_H
