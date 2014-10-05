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
#include "util/log.h"
#include "util/thread.h"
#include <cassert>

#include "jackclient.h"

using namespace std;

list<CJackClient*> CJackClient::m_clientinstances;

CJackClient::CJackClient(const std::string& name, const std::string& logname,
                         const std::string& threadname, const char* sender /*= "jack client"*/):
  CMessagePump(sender)
{
  m_clienttype   = None;
  m_name         = name;
  m_logname      = logname;
  m_threadname   = threadname;
  m_client       = NULL;
  m_connected    = false;
  m_wasconnected = true;
  m_exitstatus   = (jack_status_t)0;

  //save a pointer to this instance, in case all clients need to be disconnected from jack
  //saving it to a static list means that the portconnect will
  //also be saved here
  m_clientinstances.push_back(this);
}

CJackClient::~CJackClient()
{
  //remove pointer
  for (list<CJackClient*>::iterator it = m_clientinstances.begin(); it != m_clientinstances.end(); it++)
  {
    if (*it == this)
    {
      m_clientinstances.erase(it);
      break;
    }
  }
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

  assert(m_clienttype != None);

  LogDebug("Connecting %s to jackd", m_logname.c_str());

  //this is set in PJackInfoShutdownCallback(), init to 0 here so we know when the jack thread has exited
  m_exitstatus = (jack_status_t)0; 
  m_exitreason.clear();

  //let the derived class set some things up
  PreConnect();

  //try to connect to jackd
  m_client = jack_client_open(m_name.substr(0, jack_client_name_size() - 1).c_str(), JackNoStartServer, NULL);
  if (m_client == NULL)
  {
    if (m_wasconnected || g_printdebuglevel)
    {
      LogError("%s error connecting to jackd: \"%s\"",
               Capitalize(m_logname).c_str(), GetErrno().c_str());
      m_wasconnected = false; //only print this to the log once
    }
    return false;
  }

  //we want to know when the jack thread shuts down, so we can restart it
  jack_on_info_shutdown(m_client, SJackInfoShutdownCallback, this);

  m_samplerate = jack_get_sample_rate(m_client);
  m_buffersize = jack_get_buffer_size(m_client);

  Log("%s connected to jackd, got name \"%s\", samplerate %i",
      Capitalize(m_logname).c_str(), jack_get_client_name(m_client), m_samplerate);

  int returnv;

  //set the thread init callback, the thread name will be set there
  returnv = jack_set_thread_init_callback(m_client, SJackThreadInitCallback, this);
  if (returnv != 0)
    LogError("%s error %i setting thread init callback: \"%s\"",
             Capitalize(m_logname).c_str(), returnv, GetErrno().c_str());

  if (m_clienttype == AudioProcessor)
  {
    //set the buffer size callback so that the pregain buffer can be reallocated
    returnv = jack_set_buffer_size_callback(m_client, SJackBufferSizeCallback, this);
    if (returnv != 0)
      LogError("%s error %i setting buffer size callback: \"%s\"",
               Capitalize(m_logname).c_str(), returnv, GetErrno().c_str());

    //enable the samplerate callback, if the samplerate changes the client is restarted
    //to update the ladspa plugin with the new samplerate
    returnv = jack_set_sample_rate_callback(m_client, SJackSamplerateCallback, this);
    if (returnv != 0)
      LogError("%s error %i setting samplerate callback: \"%s\"",
               Capitalize(m_logname).c_str(), returnv, GetErrno().c_str());

    //SJackProcessCallback gets called when jack has new audio data to process
    returnv = jack_set_process_callback(m_client, SJackProcessCallback, this);
    if (returnv != 0)
    {
      LogError("%s error %i setting process callback: \"%s\"",
               Capitalize(m_logname).c_str(), returnv, GetErrno().c_str());
      return false;
    }
  }
  else
  {
    //enable port registration callback, so we know when to connect new ports
    returnv = jack_set_port_registration_callback(m_client, SJackPortRegistrationCallback, this);
    if (returnv != 0)
      LogError("%s error %i setting port registration callback: \"%s\"",
               Capitalize(m_logname).c_str(), returnv, GetErrno().c_str());

    //enable port connect callback, so we know when to disconnect ports
    returnv = jack_set_port_connect_callback(m_client, SJackPortConnectCallback, this);
    if (returnv != 0)
      LogError("%s error %i setting port connect callback: \"%s\"",
               Capitalize(m_logname).c_str(), returnv, GetErrno().c_str());
  }

  //let the derived class make jack ports
  if (!PreActivate())
    return false;

  //everything set up, activate
  returnv = jack_activate(m_client);
  if (returnv != 0)
  {
    LogError("%s error %i activating client: \"%s\"",
             Capitalize(m_logname).c_str(), returnv, GetErrno().c_str());
    return false;
  }

  //let the portconnector request a list of ports
  if (!PostActivate())
    return false;

  return true;
}

void CJackClient::Disconnect()
{
  if (m_client)
  {
    //deactivate the client before everything else
    int returnv = jack_deactivate(m_client);
    if (returnv != 0)
      LogError("%s error %i deactivating client: \"%s\"",
               Capitalize(m_logname).c_str(), returnv, GetErrno().c_str());

    //let the derived class clean up its ports
    PostDeactivate();

    //close the jack client
    returnv = jack_client_close(m_client);
    if (returnv != 0)
      LogError("%s error %i closing client: \"%s\"",
               Capitalize(m_logname).c_str(), returnv, GetErrno().c_str());

    m_client = NULL;
  }

  m_connected  = false;
  m_exitstatus = (jack_status_t)0;
  m_samplerate = 0;
  m_buffersize = 0;
}

void CJackClient::DisconnectAll()
{
  //disconnect all instances of CJackClient
  for (list<CJackClient*>::iterator it = m_clientinstances.begin(); it != m_clientinstances.end(); it++)
    (*it)->Disconnect();
}

void CJackClient::CheckExitStatus()
{
  if (m_exitstatus)
  {
    LogError("%s exited with code %i reason: \"%s\"", m_logname.c_str(), (int)m_exitstatus, m_exitreason.c_str());

    //in case of jack2, if jackd exits, libjack will deallocate all the jack clients of this process when
    //jack_client_open() is called, so all the jack clients will need to be disconnected first
    if (m_exitstatus == JackFailure)
    {
      Log("Jackd seems to have exited, disconnecting all clients");
      DisconnectAll();
    }
    else
    {
      Disconnect();
    }
  }
}

void CJackClient::SJackThreadInitCallback(void *arg)
{
  ((CJackClient*)arg)->PJackThreadInitCallback();
}

void CJackClient::PJackThreadInitCallback()
{
  //set the name of the jack thread
  CThread::SetCurrentThreadName(m_threadname);
}

int CJackClient::SJackProcessCallback(jack_nframes_t nframes, void *arg)
{
  return ((CJackClient*)arg)->PJackProcessCallback(nframes);
}

int CJackClient::PJackProcessCallback(jack_nframes_t nframes)
{
  return 0;
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
  SendMessage(MsgExited);
}

void CJackClient::SJackPortRegistrationCallback(jack_port_id_t port, int reg, void *arg)
{
  ((CJackClient*)arg)->PJackPortRegistrationCallback(port, reg);
}

void CJackClient::PJackPortRegistrationCallback(jack_port_id_t port, int reg)
{
}

void CJackClient::SJackPortConnectCallback(jack_port_id_t a, jack_port_id_t b, int connect, void *arg)
{
  ((CJackClient*)arg)->PJackPortConnectCallback(a, b, connect);
}

void CJackClient::PJackPortConnectCallback(jack_port_id_t a, jack_port_id_t b, int connect)
{
}

int CJackClient::SJackSamplerateCallback(jack_nframes_t nframes, void *arg)
{
  return ((CJackClient*)arg)->PJackSamplerateCallback(nframes);
}

int CJackClient::PJackSamplerateCallback(jack_nframes_t nframes)
{
  m_samplerate = nframes;
  return 0;
}

int CJackClient::SJackBufferSizeCallback(jack_nframes_t nframes, void *arg)
{
  return ((CJackClient*)arg)->PJackBufferSizeCallback(nframes);
}

int CJackClient::PJackBufferSizeCallback(jack_nframes_t nframes)
{
  m_buffersize = nframes;
  return 0;
}

