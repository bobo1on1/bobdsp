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
#include "util/floatbufferops.h"
#include "ladspainstance.h"

#include <cstdlib>
#include <cstring>
#include <assert.h>

using namespace std;

CPort::CPort(jack_port_t* jackport, unsigned long ladspaport, bool isinput)
{
  m_jackport   = jackport;
  m_ladspaport = ladspaport;
  m_isinput    = isinput;
  m_buf        = NULL;
}

CPort::~CPort()
{
  free(m_buf);
}

void CPort::AllocateBuffer(int buffersize)
{
  free(m_buf);

  //allocate a buffer aligned to 16 bytes so sse vector instructions can be used
  //make it 16 bytes larger than needed so the offset can be adjusted
  posix_memalign((void**)&m_buf, 16, buffersize * sizeof(float) + 16);
}

float* CPort::GetBuffer(float* jackptr)
{
  //return a pointer to the buffer, and give it the same offset from a 16 byte alignment
  //as the pointer to the jack buffer
  //when copying from one buffer to the other, both buffers will be aligned to 16 bytes
  //after at most 3 iterations, assuming that the jack pointer is at least aligned to 4 bytes
  //this way, sse vector instructions can be used
  uintptr_t offset = (uintptr_t)jackptr & 15;
  return (float*)((uintptr_t)m_buf + offset);
}

CLadspaInstance::CLadspaInstance(jack_client_t* client, const std::string& name, int instance, int totalinstances, 
    CLadspaPlugin* plugin, controlmap& controlinputs, int samplerate, int buffersize) :
  m_controlinputs(controlinputs)
{
  m_client         = client;
  m_name           = name;
  m_instance       = instance;
  m_totalinstances = totalinstances;
  m_plugin         = plugin;
  m_samplerate     = samplerate;
  m_buffersize     = buffersize;
  m_activated      = false;
  m_handle         = NULL;
}

CLadspaInstance::~CLadspaInstance()
{
  Disconnect();
}

bool CLadspaInstance::Connect()
{
  //instantiate the ladspa plugin
  m_handle = m_plugin->Descriptor()->instantiate(m_plugin->Descriptor(), m_samplerate);
  if (!m_handle)
  {
    LogError("Instantiate of client \"%s\" plugin \"%s\" failed", m_name.c_str(), m_plugin->Label());
    return false;
  }

  //connect the control ports
  for (unsigned long port = 0; port < m_plugin->PortCount(); port++)
  {
    if (m_plugin->IsControl(port))
    {
      if (m_plugin->IsOutput(port))
      {
        //only allocate a value for the output port
        //since any next call might do a realloc, and invalidate the pointer
        m_controloutputs[m_plugin->PortName(port)] = 0.0;
      }
      else
      {
        controlmap::iterator it = m_controlinputs.find(m_plugin->PortName(port));
        assert(it != m_controlinputs.end());
        m_plugin->Descriptor()->connect_port(m_handle, port, it->second);
      }
    }
  }

  //connect output control ports here, when all values have been allocated
  for (unsigned long port = 0; port < m_plugin->PortCount(); port++)
  {
    if (m_plugin->IsControlOutput(port))
    {
      controlmap::iterator it = m_controloutputs.find(m_plugin->PortName(port));
      assert(it != m_controloutputs.end());
      m_plugin->Descriptor()->connect_port(m_handle, port, it->second);
    }
  }

  //activate the ladspa plugin
  Activate();

  //create jack ports for each audio port of the ladspa plugin
  for (unsigned long ladspaport = 0; ladspaport < m_plugin->PortCount(); ladspaport++)
  {
    if (m_plugin->IsAudio(ladspaport))
    {
      int portflags;
      if (m_plugin->IsInput(ladspaport))
        portflags = JackPortIsInput;
      else
        portflags = JackPortIsOutput;

      string portname = m_plugin->PortName(ladspaport);
      string strinstance;
      if (m_totalinstances > 1)
        strinstance = string("_") + ToString(m_instance + 1);
      portname = portname.substr(0, jack_port_name_size() - strinstance.length() - 1) + strinstance;

      jack_port_t* jackport = jack_port_register(m_client, portname.c_str(), JACK_DEFAULT_AUDIO_TYPE, portflags, 0);
      if (jackport == NULL)
      {
        LogError("Client \"%s\" error registering jack port \"%s\": \"%s\"",
                 m_name.c_str(), portname.c_str(), GetErrno().c_str());
        return false;
      }

      m_ports.push_back(CPort(jackport, ladspaport, m_plugin->IsInput(ladspaport)));
    }
  }

  AllocateBuffers(m_buffersize);

  return true;
}

void CLadspaInstance::Disconnect(bool unregisterjack /*= true*/)
{
  //deactivate and clean up the ladspa plugin
  Deactivate();
  if (m_handle)
  {
    m_plugin->Descriptor()->cleanup(m_handle);
    m_handle = NULL;
  }

  //close the jack ports
  while (!m_ports.empty())
  {
    if (unregisterjack)
    {
      int returnv = jack_port_unregister(m_client, m_ports.back().GetJackPort());
      if (returnv != 0)
        LogError("Client \"%s\" error %i unregistering port: \"%s\"",
                  m_name.c_str(), returnv, GetErrno().c_str());
    }

    m_ports.pop_back();
  }
}

void CLadspaInstance::Activate()
{
  if (m_handle && !m_activated)
  {
    if (m_plugin->Descriptor()->activate)
      m_plugin->Descriptor()->activate(m_handle);

    m_activated = true;
  }
}

void CLadspaInstance::Deactivate()
{
  if (m_handle && m_activated)
  {
    if (m_plugin->Descriptor()->deactivate)
      m_plugin->Descriptor()->deactivate(m_handle);

    m_activated = false;
  }
}

void CLadspaInstance::AllocateBuffers(int buffersize)
{
  m_buffersize = buffersize;
  for (vector<CPort>::iterator it = m_ports.begin(); it != m_ports.end(); it++)
    it->AllocateBuffer(buffersize);
}

//this is called from the jack client thread
void CLadspaInstance::Run(jack_nframes_t nframes, float pregain, float postgain)
{
  for (vector<CPort>::iterator it = m_ports.begin(); it != m_ports.end(); it++)
  {
    float* jackptr = (float*)jack_port_get_buffer(it->GetJackPort(), nframes);

    if (it->IsInput())
    {
      //when input gain is needed, apply and copy audio to the temp buffer
      if (pregain != 1.0f)
      {
        float* buf = it->GetBuffer(jackptr);
        CopyApplyGain(jackptr, buf, nframes, pregain);

        //connect the ladspa port to the temp buffer
        m_plugin->Descriptor()->connect_port(m_handle, it->GetLadspaPort(), buf);
      }
      else
      {
        //no gain needed, connect the ladspa port directly to the jack buffer
        m_plugin->Descriptor()->connect_port(m_handle, it->GetLadspaPort(), jackptr);
      }
    }
    else
    {
      //connect the ladspa output port to the jack output port
      //gain is applied directly on the jack output buffer afterwards
      m_plugin->Descriptor()->connect_port(m_handle, it->GetLadspaPort(), jackptr);
    }
  }

  //run the ladspa plugin on the audio data
  m_plugin->Descriptor()->run(m_handle, nframes);

  //apply gain for all jack output ports
  if (postgain != 1.0f)
  {
    for (vector<CPort>::iterator it = m_ports.begin(); it != m_ports.end(); it++)
    {
      if (!it->IsInput())
        ApplyGain((float*)jack_port_get_buffer(it->GetJackPort(), nframes), nframes, postgain);
    }
  }
}

