/*
 * bobdsp
 * Copyright (C) Bob 2013
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

#include "clientmessage.h"
#include "util/log.h"
#include "util/misc.h"

#ifndef _GNU_SOURCE
  #define _GNU_SOURCE //for pipe2
#endif //_GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>

const char* MsgToString(ClientMessage msg)
{
  static const char* msgstrings[] = 
  {
    "MsgNone",
    "MsgExited",
    "MsgPortRegistered",
    "MsgPortDeregistered",
    "MsgPortConnected",
    "MsgPortDisconnected",
    "MsgSamplerateChanged",
    "MsgConnectionsUpdated",
    "MsgCheckClients",
  };

  if (msg >= 0 && (size_t)msg < (sizeof(msgstrings) / sizeof(msgstrings[0])))
    return msgstrings[msg];
  else
    return "ERROR: INVALID MESSAGE";
}

CMessagePump::CMessagePump(const char* sender)
{
  m_sender = sender;

  if (pipe2(m_pipe, O_NONBLOCK) == -1)
  {
    LogError("creating msg pipe for %s: %s", m_sender, GetErrno().c_str());
    m_pipe[0] = m_pipe[1] = -1;
  }

  memset((void*)m_msgstates, 0, sizeof(m_msgstates));
}

CMessagePump::~CMessagePump()
{
  if (m_pipe[0] != -1)
    close(m_pipe[0]);
  if (m_pipe[1] != -1)
    close(m_pipe[1]);
}

ClientMessage CMessagePump::GetMessage()
{
  if (m_pipe[0] == -1)
    return MsgNone;

  uint8_t msg;
  int returnv = read(m_pipe[0], &msg, 1);
  if (returnv == 1)
  {
    return (ClientMessage)msg;
  }
  else if (returnv == -1 && errno != EAGAIN)
  {
    int tmperrno = errno;
    LogError("%s error reading msg from pipe: \"%s\"", m_sender, GetErrno().c_str());
    if (tmperrno != EINTR)
    {
      close(m_pipe[0]);
      m_pipe[0] = -1;
    }
  }

  return MsgNone;
}

bool CMessagePump::WriteMessage(ClientMessage msg)
{
  if (m_pipe[1] == -1)
  {
    LogError("%s message pipe closed", m_sender);
    return true; //can't write
  }

  uint8_t msgbyte = msg;
  int returnv = write(m_pipe[1], &msgbyte, 1);
  if (returnv == 1)
    return true; //write successful

  if (returnv == -1)
  {
    int tmperrno = errno;
    LogError("%s error writing msg %s to pipe: \"%s\"", m_sender, MsgToString(msg), GetErrno().c_str());
    if (tmperrno != EINTR && tmperrno != EAGAIN)
    {
      close(m_pipe[1]); //pipe broken, close it
      m_pipe[1] = -1;
      return true;
    }
  }

  return false; //need to try again
}

bool CMessagePump::SendMessage(ClientMessage msg)
{
  //a message here will only be sent if the previous message
  //of the same type has been confirmed
  //this is used to prevent flooding the main thread
  //with jack messages
  if (MsgCAS(m_msgstates + msg, 0, 1))
  {
    if (WriteMessage(msg))
    {
      return true;
    }
    else
    {
      //if writing the message fails, set the state back to 0
      MsgCAS(m_msgstates + msg, 1, 0);
      return false;
    }
  }
  else
  {
    return true;
  }
}

void CMessagePump::ConfirmMessage(ClientMessage msg)
{
  MsgCAS(m_msgstates + msg, 1, 0);
  ProcessMessage(msg);
}
