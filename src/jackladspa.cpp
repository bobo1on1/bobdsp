
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

#include <cassert>

#include "util/inclstdint.h"
#include "util/misc.h"
#include "util/lock.h"
#include "jackladspa.h"

using namespace std;

CJackLadspa::CJackLadspa(CLadspaPlugin* plugin, const std::string& name, int nrinstances,
                         double* gain, controlmap controlinputs):
  CJackClient(name, string("client \"") + name + "\"", name)
{
  m_clienttype    = AudioProcessor;
  m_plugin        = plugin;
  m_nrinstances   = nrinstances;
  m_controlinputs = controlinputs;
  m_delete        = false;
  m_restart       = false;
  m_samplerate    = 0;
  m_buffersize    = 0;

  for (int i = 0; i < 2; i++)
  {
    m_gain[i] = gain[i];
    m_runninggain[i] = gain[i];

    m_runninggain[i].Update();
    m_runninggain[i].SetSmooth(true);
  }

  for (unsigned long port = 0; port < m_plugin->PortCount(); port++)
  {
    if (m_plugin->IsControlInput(port))
    {
      controlmap::iterator it = m_controlinputs.find(m_plugin->PortName(port));
      assert(it != m_controlinputs.end());

      //apply the value before starting
      it->second.Update();

      //enable smooth transitions for input controls that are not integer or boolean
      if (!m_plugin->IsInteger(port) && !m_plugin->IsToggled(port))
        it->second.SetSmooth(true);
    }
  }
}

CJackLadspa::~CJackLadspa()
{
  Disconnect();
}

void CJackLadspa::PreConnect()
{
  //load all symbols, so it doesn't have to be done from the jack thread
  //this is better for realtime performance
  m_plugin->LoadAllSymbols();
}

bool CJackLadspa::PreActivate()
{
  //allocate plugin instances
  for (int instance = 0; instance < m_nrinstances; instance++)
  {
    CLadspaInstance* ladspainstance = new CLadspaInstance(m_client, m_name, instance, m_nrinstances, m_plugin,
                                                          m_controlinputs, m_samplerate, m_buffersize);
    m_instances.push_back(ladspainstance);
  }

  //every CLadspaInstance makes its own jack ports
  for (vector<CLadspaInstance*>::iterator it = m_instances.begin(); it != m_instances.end(); it++)
  {
    if (!(*it)->Connect())
      return false;
  }

  return true;
}

void CJackLadspa::PostDeactivate()
{
  //clean up the plugin instances
  while(!m_instances.empty())
  {
    m_instances.back()->Disconnect();
    delete m_instances.back();
    m_instances.pop_back();
  }
}

void CJackLadspa::UpdateGain(double gain, int index)
{
  //update the gain, this will be read from the jack thread
  CLock lock(m_mutex);
  m_gain[index] = gain;
}

void CJackLadspa::GetControlInputs(controlmap& controlinputs)
{
  CLock lock(m_mutex);
  //copy the control inputs, then apply any pending updates to it
  //don't clear the updates, the jack client needs them
  controlinputs = m_controlinputs;
  TransferNewControlInputs(controlinputs);
}

void CJackLadspa::UpdateControls(controlmap& controlinputs)
{
  //store the new control values, these will be read from the jack thread
  CLock lock(m_mutex);
  for (controlmap::iterator it = controlinputs.begin(); it != controlinputs.end(); it++)
    m_newcontrolinputs[it->first] = it->second;
}

void CJackLadspa::TransferNewControlInputs(controlmap& controlinputs)
{
  for (controlmap::iterator it = m_newcontrolinputs.begin();
      it != m_newcontrolinputs.end(); it++)
  {
    controlmap::iterator control = controlinputs.find(it->first);
    assert(control != controlinputs.end());
    control->second = it->second;
  }
}

bool CJackLadspa::NeedsSmooth()
{
  for (int i = 0; i < 2; i++)
  {
    if (m_runninggain[i].NeedsSmooth())
      return true;
  }

  for (controlmap::iterator it = m_controlinputs.begin(); it != m_controlinputs.end(); it++)
  {
    if (it->second.NeedsSmooth())
      return true;
  }

  return false;
}

#define SMOOTHBLOCK 0.001f
#define SMOOTHTIME  0.05f
int CJackLadspa::PJackProcessCallback(jack_nframes_t nframes)
{
  //check if gain was updated, use a trylock to prevent blocking the realtime jack thread
  CLock lock(m_mutex, true);
  if (lock.HasLock())
  {
    //update the gain, but only when it changed, otherwise m_floatorig
    //in the controlvalue class gets overwritten
    for (int i = 0; i < 2; i++)
    {
      if (m_gain[i] != (double)m_runninggain[i])
        m_runninggain[i] = m_gain[i];
    }

    //apply updates to control values if there are any
    TransferNewControlInputs(m_controlinputs);
    m_newcontrolinputs.clear();

    lock.Leave();
  }

  //chose a blocksize that is one millisecond of samples, then round up
  //to the nearest multiple of 4
  int blocksize = Max(Round32(SMOOTHBLOCK * m_samplerate), 4);
  if ((blocksize & 3) != 0)
    blocksize = (blocksize & ~3) + 4;

  //process audio in small blocks with control smoothing when necessary
  int processed = 0;
  while (NeedsSmooth() && processed < (int)nframes)
  {
    int   process = Min((int)nframes - processed, blocksize);
    float smoothval = ((float)process / SMOOTHTIME) / m_samplerate;

    //move each gain value towards its target
    for (int i = 0; i < 2; i++)
      m_runninggain[i].Update(smoothval);

    //move each control value towards its target
    for (controlmap::iterator it = m_controlinputs.begin(); it != m_controlinputs.end(); it++)
      it->second.Update(smoothval);

    //process a small block of audio
    for (vector<CLadspaInstance*>::iterator it = m_instances.begin(); it != m_instances.end(); it++)
      (*it)->Run(nframes, process, processed, m_runninggain[0].FloatVal(), m_runninggain[1].FloatVal());

    processed += process;
  }

  //process remaining audio without smoothing the controls
  if (processed < (int)nframes)
  {
    //update gain
    for (int i = 0; i < 2; i++)
      m_runninggain[i].Update();

    //update controls
    for (controlmap::iterator it = m_controlinputs.begin(); it != m_controlinputs.end(); it++)
      it->second.Update();

    //process the remaining audio
    for (vector<CLadspaInstance*>::iterator it = m_instances.begin(); it != m_instances.end(); it++)
      (*it)->Run(nframes, nframes - processed, processed, m_runninggain[0].FloatVal(), m_runninggain[1].FloatVal());
  }

  return 0;
}

int CJackLadspa::PJackSamplerateCallback(jack_nframes_t nframes)
{
  if ((int)nframes != m_samplerate)
  {
    //if the jack samplerate changes, restart the client
    MarkRestart();

    //signal the main thread that this thread needs a restart
    SendMessage(MsgSamplerateChanged);
  }

  return 0;
}

int CJackLadspa::PJackBufferSizeCallback(jack_nframes_t nframes)
{
  if ((int)nframes != m_buffersize)
  {
    m_buffersize = nframes;
    for (vector<CLadspaInstance*>::iterator it = m_instances.begin(); it != m_instances.end(); it++)
      (*it)->AllocateBuffers(m_buffersize);
  }

  return 0;
}

