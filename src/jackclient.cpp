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

#include "util/inclstdint.h"
#include "util/misc.h"
#include "util/timeutils.h"
#include "util/log.h"
#include "util/thread.h"
#include "util/lock.h"
#include <assert.h>

#include "jackclient.h"

#define PORTEVENT_REGISTERED   0x01
#define PORTEVENT_DEREGISTERED 0x02
#define PORTEVENT_CONNECTED    0x04
#define PORTEVENT_DISCONNECTED 0x08
#define SAMPLERATE_CHANGED     0x10

using namespace std;

CJackClient::CJackClient(CLadspaPlugin* plugin, const std::string& name, int nrinstances,
                         double* gain, controlmap controlinputs):
  CMessagePump("jack client")
{
  m_plugin        = plugin;
  m_name          = name;
  m_nrinstances   = nrinstances;
  m_controlinputs = controlinputs;
  m_client        = NULL;
  m_connected     = false;
  m_wasconnected  = true;
  m_delete        = false;
  m_restart       = false;
  m_exitstatus    = (jack_status_t)0;
  m_samplerate    = 0;
  m_events        = 0;

  for (int i = 0; i < 2; i++)
  {
    m_gain[i] = gain[i];
    m_runninggain[i] = gain[i];
  }
}

CJackClient::~CJackClient()
{
  Disconnect();
}

bool CJackClient::Connect()
{
  m_connected = ConnectInternal();
  if (!m_connected)
    Disconnect();
  else
    m_wasconnected = true;

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

  //load all symbols, so it doesn't have to be done from the jack thread
  //this is better for realtime performance
  m_plugin->LoadAllSymbols();

  //try to connect to jackd
  m_client = jack_client_open(m_name.substr(0, jack_client_name_size() - 1).c_str(), JackNoStartServer, NULL);
  if (m_client == NULL)
  {
    if (m_wasconnected || g_printdebuglevel)
    {
      LogError("Client \"%s\" error connecting to jackd: \"%s\"", m_name.c_str(), GetErrno().c_str());
      m_wasconnected = false; //only print this to the log once
    }
    return false;
  }

  //we want to know when the jack thread shuts down, so we can restart it
  jack_on_info_shutdown(m_client, SJackInfoShutdownCallback, this);

  m_samplerate = jack_get_sample_rate(m_client);

  Log("Client \"%s\" connected to jackd, got name \"%s\", samplerate %i",
      m_name.c_str(), jack_get_client_name(m_client), m_samplerate);

  int returnv;

  //set the thread init callback, the thread name will be set there
  returnv = jack_set_thread_init_callback(m_client, SJackThreadInitCallback, this);
  if (returnv != 0)
    LogError("Client \"%s\" error %i setting thread init callback: \"%s\"",
             m_name.c_str(), returnv, GetErrno().c_str());

  //enable the samplerate callback, if the samplerate changes the client is restarted
  //to update the ladspa plugin with the new samplerate
  returnv = jack_set_sample_rate_callback(m_client, SJackSamplerateCallback, this);
  if (returnv != 0)
    LogError("Client \"%s\" error %i setting samplerate callback: \"%s\"",
             m_name.c_str(), returnv, GetErrno().c_str());

  //enable port registration callback, so we know when to connect new ports
  returnv = jack_set_port_registration_callback(m_client, SJackPortRegistrationCallback, this);
  if (returnv != 0)
    LogError("Client \"%s\" error %i setting port registration callback: \"%s\"",
             m_name.c_str(), returnv, GetErrno().c_str());

  //enable port connect callback, so we know when to disconnect ports
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
      //don't unregister the jack port here when the jack client exited, this might hang in libjack
      m_instances.back()->Disconnect(m_exitstatus == 0);
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
  m_events     = 0;
  m_exitstatus = (jack_status_t)0;
  m_samplerate = 0;

  //if this client was marked for restart, reset the flag
  m_restart = false;
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
    CLadspaInstance* ladspainstance = new CLadspaInstance(m_client, m_name, instance, m_nrinstances,
                                                          m_plugin, m_controlinputs, m_samplerate);
    m_instances.push_back(ladspainstance);
  }
}

void CJackClient::UpdateGain(float gain, int index)
{
  //update the gain, this will be read from the jack thread
  CLock lock(m_mutex);
  m_gain[index] = gain;
}

void CJackClient::GetControlInputs(controlmap& controlinputs)
{
  CLock lock(m_mutex);
  //copy the control inputs, then apply any pending updates to it
  //don't clear the updates, the jack client needs them
  controlinputs = m_controlinputs;
  TransferNewControlInputs(controlinputs);
}

void CJackClient::UpdateControls(controlmap& controlinputs)
{
  //store the new control values, these will be read from the jack thread
  CLock lock(m_mutex);
  for (controlmap::iterator it = controlinputs.begin(); it != controlinputs.end(); it++)
    m_newcontrolinputs[it->first] = it->second;
}

void CJackClient::CheckMessages()
{
  if (m_events)
  {
    if ((m_events & PORTEVENT_REGISTERED) && WriteMessage(MsgPortRegistered))
      m_events &= ~PORTEVENT_REGISTERED;

    if ((m_events & PORTEVENT_DEREGISTERED) && WriteMessage(MsgPortDeregistered))
      m_events &= ~PORTEVENT_DEREGISTERED;

    if ((m_events & PORTEVENT_CONNECTED) && WriteMessage(MsgPortConnected))
      m_events &= ~PORTEVENT_CONNECTED;

    if ((m_events & PORTEVENT_DISCONNECTED) && WriteMessage(MsgPortDisconnected))
      m_events &= ~PORTEVENT_DISCONNECTED;

    if ((m_events & SAMPLERATE_CHANGED) && WriteMessage(MsgSamplerateChanged))
      m_events &= ~SAMPLERATE_CHANGED;
  }
}

void CJackClient::TransferNewControlInputs(controlmap& controlinputs)
{
  for (controlmap::iterator it = m_newcontrolinputs.begin();
      it != m_newcontrolinputs.end(); it++)
  {
    controlmap::iterator control = controlinputs.find(it->first);
    assert(control != controlinputs.end());
    control->second = it->second;
  }
}

void CJackClient::SJackThreadInitCallback(void *arg)
{
  ((CJackClient*)arg)->PJackThreadInitCallback();
}

void CJackClient::PJackThreadInitCallback()
{
  //set the name of the jack thread
  CThread::SetCurrentThreadName(m_name);
}

int CJackClient::SJackProcessCallback(jack_nframes_t nframes, void *arg)
{
  ((CJackClient*)arg)->PJackProcessCallback(nframes);
  return 0;
}

void CJackClient::PJackProcessCallback(jack_nframes_t nframes)
{
  //check if gain was updated, use a trylock to prevent blocking the realtime jack thread
  CLock lock(m_mutex, true);
  if (lock.HasLock())
  {
    //update the gain
    for (int i = 0; i < 2; i++)
      m_runninggain[i] = m_gain[i];

    //apply updates to control values if there are any
    TransferNewControlInputs(m_controlinputs);
    m_newcontrolinputs.clear();

    lock.Leave();
  }

  //process audio
  for (vector<CLadspaInstance*>::iterator it = m_instances.begin(); it != m_instances.end(); it++)
    (*it)->Run(nframes, m_runninggain[0], m_runninggain[1]);

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

  //tell the main loop this client has exited
  WriteMessage(MsgExited);
}

void CJackClient::SJackPortRegistrationCallback(jack_port_id_t port, int reg, void *arg)
{
  ((CJackClient*)arg)->PJackPortRegistrationCallback(port, reg);
}

void CJackClient::PJackPortRegistrationCallback(jack_port_id_t port, int reg)
{
  if (reg)
    m_events |= PORTEVENT_REGISTERED;
  else
    m_events |= PORTEVENT_DEREGISTERED;

  CheckMessages();
}

void CJackClient::SJackPortConnectCallback(jack_port_id_t a, jack_port_id_t b, int connect, void *arg)
{
  ((CJackClient*)arg)->PJackPortConnectCallback(a, b, connect);
}

void CJackClient::PJackPortConnectCallback(jack_port_id_t a, jack_port_id_t b, int connect)
{
  if (connect)
    m_events |= PORTEVENT_CONNECTED;
  else
    m_events |= PORTEVENT_DISCONNECTED;

  CheckMessages();
}

int CJackClient::SJackSamplerateCallback(jack_nframes_t nframes, void *arg)
{
  return ((CJackClient*)arg)->PJackSamplerateCallback(nframes);
}

int CJackClient::PJackSamplerateCallback(jack_nframes_t nframes)
{
  if ((int)nframes != m_samplerate)
  {
    //if the jack samplerate changes, restart the client
    MarkRestart();

    //send a message to the main thread that the samplerate changed
    m_events |= SAMPLERATE_CHANGED;
    CheckMessages();
  }

  return 0;
}

