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

#ifndef _GNU_SOURCE
  #define _GNU_SOURCE //for pipe2
  #include <unistd.h>
#endif //_GNU_SOURCE
#include <fcntl.h>

#include "util/inclstdint.h"
#include "util/misc.h"
#include "util/log.h"

#include "jackclient.h"

using namespace std;

CJackClient::CJackClient(CLadspaPlugin* plugin, const std::string& name, int nrinstances,
                         float pregain, float postgain, std::vector<portvalue> controlinputs,
                         const std::string& clientprefix, const std::string& portprefix)
{
  m_plugin          = plugin;
  m_name            = name;
  m_nrinstances     = nrinstances;
  m_pregain         = pregain;
  m_postgain        = postgain;
  m_controlinputs   = controlinputs;
  m_client          = NULL;
  m_connected       = false;
  m_exitstatus      = (jack_status_t)0;
  m_clientprefix    = clientprefix;
  m_portprefix      = portprefix;
  m_samplerate      = 0;
  m_portregistered  = false;
  m_portconnected   = false;

  //load all symbols, so it doesn't have to be done from the jack thread
  //this is better for realtime performance
  m_plugin->LoadAllSymbols();

  if (pipe2(m_pipe, O_NONBLOCK) == -1)
  {
    LogError("creating msg pipe for client \"%s\": %s", m_name.c_str(), GetErrno().c_str());
    m_pipe[0] = m_pipe[1] = -1;
  }
}

CJackClient::~CJackClient()
{
  Disconnect();

  if (m_pipe[0] != -1)
    close(m_pipe[0]);
  if (m_pipe[1] != -1)
    close(m_pipe[1]);
}

bool CJackClient::Connect()
{
  m_connected = ConnectInternal();
  if (!m_connected)
    Disconnect();

  return m_connected;
}

bool CJackClient::ConnectInternal()
{
  if (m_connected)
    return true; //already connected

  LogDebug("Connecting client \"%s\" to jackd", m_name.c_str());

  //this is set in PJackInfoShutdownCallback(), init to 0 here so we know when the jack thread has exited
  m_exitstatus = (jack_status_t)0; 
  m_exitreason.clear();

  //try to connect to jackd
  string name = m_clientprefix + m_name;
  m_client = jack_client_open(name.substr(0, jack_client_name_size() + 1).c_str(), JackNoStartServer, NULL);
  if (m_client == NULL)
  {
    LogError("Client \"%s\" error connecting to jackd: \"%s\"", m_name.c_str(), GetErrno().c_str());
    return false;
  }

  //we want to know when the jack thread shuts down, so we can restart it
  jack_on_info_shutdown(m_client, SJackInfoShutdownCallback, this);

  m_samplerate = jack_get_sample_rate(m_client);

  Log("Client \"%s\" connected to jackd, got name \"%s\", samplerate %" PRIi32,
      m_name.c_str(), jack_get_client_name(m_client), m_samplerate);

  int returnv;

  //enable port registration callback, so we know when to connect new ports
  m_portregistered = false;
  returnv = jack_set_port_registration_callback(m_client, SJackPortRegistrationCallback, this);
  if (returnv != 0)
    LogError("Client \"%s\" error %i setting port registration callback: \"%s\"",
             m_name.c_str(), returnv, GetErrno().c_str());

  //enable port connect callback, so we know when to disconnect ports
  m_portconnected = false;
  returnv = jack_set_port_connect_callback(m_client, SJackPortConnectCallback, this);
  if (returnv != 0)
    LogError("Client \"%s\" error %i setting port connect callback: \"%s\"",
             m_name.c_str(), returnv, GetErrno().c_str());

  //SJackProcessCallback gets called when jack has new audio data to process
  returnv = jack_set_process_callback(m_client, SJackProcessCallback, this);
  if (returnv != 0)
  {
    LogError("Client \"%s\" error %i setting process callback: \"%s\"",
             m_name.c_str(), returnv, GetErrno().c_str());
    return false;
  }

  InitLadspa();

  if (!ConnectJackPorts())
    return false;

  //everything set up, activate
  returnv = jack_activate(m_client);
  if (returnv != 0)
  {
    LogError("Client \"%s\" error %i activating client: \"%s\"",
             m_name.c_str(), returnv, GetErrno().c_str());
    return false;
  }

  return true;
}

void CJackClient::Disconnect()
{
  if (m_client)
  {
    //deactivate the client before everything else
    int returnv = jack_deactivate(m_client);
    if (returnv != 0)
      LogError("Client \"%s\" error %i deactivating client: \"%s\"",
               m_name.c_str(), returnv, GetErrno().c_str());

    //clean up the plugin instances
    while(!m_instances.empty())
    {
      delete m_instances.back();
      m_instances.pop_back();
    }

    //close the jack client
    returnv = jack_client_close(m_client);
    if (returnv != 0)
      LogError("Client \"%s\" error %i closing client: \"%s\"",
               m_name.c_str(), returnv, GetErrno().c_str());

    m_client = NULL;
  }

  m_connected  = false;
  m_exitstatus = (jack_status_t)0;
}

bool CJackClient::ConnectJackPorts()
{
  //every CLadspaInstance makes its own jack ports
  for (vector<CLadspaInstance*>::iterator it = m_instances.begin(); it != m_instances.end(); it++)
  {
    if (!(*it)->Connect())
      return false;
  }

  return true;
}

void CJackClient::InitLadspa()
{
  //allocate plugin instances
  for (int instance = 0; instance < m_nrinstances; instance++)
  {
    CLadspaInstance* ladspainstance = new CLadspaInstance(m_client, m_name, instance, m_nrinstances, m_plugin,
                                                          m_controlinputs, m_samplerate, m_portprefix);
    m_instances.push_back(ladspainstance);
  }
}

void CJackClient::CheckMessages()
{
  if (m_portregistered)
    m_portregistered = !WriteMessage(MsgPortRegistered);
  if (m_portconnected)
    m_portconnected  = !WriteMessage(MsgPortConnected);
}

//returns true when the message has been sent or pipe is broken
//returns false when the message write needs to be retried
bool CJackClient::WriteMessage(uint8_t msg)
{
  if (m_pipe[1] == -1)
    return true; //can't write

  int returnv = write(m_pipe[1], &msg, 1);
  if (returnv == 1)
    return true; //write successful

  if (returnv == -1 && errno != EAGAIN)
  {
    LogError("Client \"%s\" error writing msg %i to pipe: \"%s\"", m_name.c_str(), msg, GetErrno().c_str());
    close(m_pipe[1]);
    m_pipe[1] = -1;
    return true; //pipe broken
  }

  return false; //need to try again
}

ClientMessage CJackClient::GetMessage()
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
    LogError("Client \"%s\" error reading msg from pipe: \"%s\"", m_name.c_str(), GetErrno().c_str());
    close(m_pipe[0]);
    m_pipe[0] = -1;
  }

  return MsgNone;
}

int CJackClient::SJackProcessCallback(jack_nframes_t nframes, void *arg)
{
  ((CJackClient*)arg)->PJackProcessCallback(nframes);
  return 0;
}

void CJackClient::PJackProcessCallback(jack_nframes_t nframes)
{
  //process audio
  for (vector<CLadspaInstance*>::iterator it = m_instances.begin(); it != m_instances.end(); it++)
    (*it)->Run(nframes, m_pregain, m_postgain);

  //check if we need to send a message to the main loop
  CheckMessages();
}

void CJackClient::SJackInfoShutdownCallback(jack_status_t code, const char *reason, void *arg)
{
  ((CJackClient*)arg)->PJackInfoShutdownCallback(code, reason);
}

void CJackClient::PJackInfoShutdownCallback(jack_status_t code, const char *reason)
{
  //save the exit code, this will be read from the loop in main()
  //make sure reason is saved before code, to make it thread safe
  //since main() will read m_exitstatus first, then m_exitreason if necessary
  m_exitreason = reason;
  m_exitstatus = code;

  //send message to the main loop
  if (!WriteMessage(MsgExited))
    LogError("Client \"%s\" unable to write exit msg to pipe", m_name.c_str());
}

void CJackClient::SJackPortRegistrationCallback(jack_port_id_t port, int reg, void *arg)
{
  ((CJackClient*)arg)->PJackPortRegistrationCallback(port, reg);
}

void CJackClient::PJackPortRegistrationCallback(jack_port_id_t port, int reg)
{
  if (reg)
  {
    //only message to the main loop when a client registers
    //if a client deregisters is not important
    m_portregistered = true;
    CheckMessages();
  }
}

void CJackClient::SJackPortConnectCallback(jack_port_id_t a, jack_port_id_t b, int connect, void *arg)
{
  ((CJackClient*)arg)->PJackPortConnectCallback(a, b, connect);
}

void CJackClient::PJackPortConnectCallback(jack_port_id_t a, jack_port_id_t b, int connect)
{
  //message to the mainloop that a port has been connected
  if (connect)
  {
    m_portconnected = true;
    CheckMessages();
  }
}

