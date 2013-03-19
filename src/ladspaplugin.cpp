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

#include <dlfcn.h>
#include <cstring>
#include "ladspaplugin.h"
#include "util/log.h"
#include "util/misc.h"

using namespace std;

CLadspaPlugin::CLadspaPlugin(const std::string& filename, void* handle, const LADSPA_Descriptor* descriptor)
{
  m_filename    = filename;
  m_handle      = handle;
  m_descriptor  = descriptor;
  m_fullyloaded = false;
}

CLadspaPlugin::~CLadspaPlugin()
{
  dlclose(m_handle);
}

bool CLadspaPlugin::Sort(CLadspaPlugin* first, CLadspaPlugin* second)
{
  //sort by name, uniqueid, and filename
  int sort = strcmp(first->Name(), second->Name());
  if (sort != 0)
  {
    return sort < 0;
  }
  else
  {
    if (first->UniqueID() < second->UniqueID())
      return true;
    else if (first->UniqueID() > second->UniqueID())
      return false;
    else
      return first->FileName() < second->FileName();
  }
}

void CLadspaPlugin::LoadAllSymbols()
{
  if (m_fullyloaded)
    return;

  //open a new handle with all symbols loaded
  void* handle = dlopen(m_filename.c_str(), RTLD_NOW);
  if (handle)
  {
    dlclose(m_handle); //close the old handle
    m_handle = handle;
    m_fullyloaded = true;
  }
  else
  {
    LogError("Unable to open new handle for %s: %s", m_filename.c_str(), dlerror());
  }
}

int CLadspaPlugin::AudioInputPorts()
{
  int ports = 0;
  for (unsigned long port = 0; port < m_descriptor->PortCount; port++)
  {
    if (IsAudioInput(port))
      ports++;
  }

  return ports;
}

int CLadspaPlugin::AudioOutputPorts()
{
  int ports = 0;
  for (unsigned long port = 0; port < m_descriptor->PortCount; port++)
  {
    if (IsAudioOutput(port))
      ports++;
  }

  return ports;
}

int CLadspaPlugin::ControlOutputPorts()
{
  int ports = 0;
  for (unsigned long port = 0; port < m_descriptor->PortCount; port++)
  {
    if (IsControlOutput(port))
      ports++;
  }

  return ports;
}

int CLadspaPlugin::ControlInputPorts()
{
  int ports = 0;
  for (unsigned long port = 0; port < m_descriptor->PortCount; port++)
  {
    if (IsControlInput(port))
      ports++;
  }

  return ports;
}

const LADSPA_PortDescriptor CLadspaPlugin::PortDescriptor(unsigned long port)
{
  if (port < PortCount())
    return m_descriptor->PortDescriptors[port];
  else
    return 0;
}

bool CLadspaPlugin::PortDescriptorSanityCheck(unsigned long port)
{
  LADSPA_PortDescriptor p = PortDescriptor(port);

  bool isinput = LADSPA_IS_PORT_INPUT(p);
  bool isoutput = LADSPA_IS_PORT_OUTPUT(p);
  bool iscontrol = LADSPA_IS_PORT_CONTROL(p);
  bool isaudio = LADSPA_IS_PORT_AUDIO(p);

  bool isok = true;

  if (!isinput && !isoutput)
  {
    LogError("Port \"%s\" of plugin %s does not have input or output flag set", PortName(port), Label());
    isok = false;
  }
  else if (isinput && isoutput)
  {
    LogError("Port \"%s\" of plugin %s has both input and output flag set", PortName(port), Label());
    isok = false;
  }

  if (!iscontrol && !isaudio)
  {
    LogError("Port \"%s\" of plugin %s does not have audio or control flag set", PortName(port), Label());
    isok = false;
  }
  else if (iscontrol && isaudio)
  {
    LogError("Port \"%s\" of plugin %s has both audio and control flag set", PortName(port), Label());
    isok = false;
  }

  return isok;
}
const LADSPA_PortRangeHint CLadspaPlugin::PortRangeHint(unsigned long port)
{
  if (port < PortCount())
  {
    return m_descriptor->PortRangeHints[port];
  }
  else
  {
    LADSPA_PortRangeHint hint = {};
    return hint;
  }
}

const char* CLadspaPlugin::PortName(unsigned long port)
{
  if (port < PortCount())
    return m_descriptor->PortNames[port];
  else
    return NULL;
}

long CLadspaPlugin::PortByName(const std::string& portname)
{
  for (unsigned long port = 0; port < PortCount(); port++)
  {
    if (portname == PortName(port))
      return port;
  }

  return -1;
}

const char* CLadspaPlugin::DirectionStr(unsigned long port)
{
  if (IsInput(port))
    return "input";
  else if (IsOutput(port))
    return "output";
  else
    return "unknown";
}

const char* CLadspaPlugin::TypeStr(unsigned long port)
{
  if (IsControl(port))
    return "control";
  else if (IsAudio(port))
    return "audio";
  else
    return "unknown";
}

bool CLadspaPlugin::IsControl(unsigned long port)
{
  if (LADSPA_IS_PORT_CONTROL(PortDescriptor(port)))
    return true;
  else
    return false;
}

bool CLadspaPlugin::IsAudio(unsigned long port)
{
  if (LADSPA_IS_PORT_AUDIO(PortDescriptor(port)))
    return true;
  else
    return false;
}

bool CLadspaPlugin::IsInput(unsigned long port)
{
  if (LADSPA_IS_PORT_INPUT(PortDescriptor(port)))
    return true;
  else
    return false;
}

bool CLadspaPlugin::IsOutput(unsigned long port)
{
  if (LADSPA_IS_PORT_OUTPUT(PortDescriptor(port)))
    return true;
  else
    return false;
}

bool CLadspaPlugin::IsControlInput(unsigned long port)
{
  return IsControl(port) && IsInput(port);
}

bool CLadspaPlugin::IsControlOutput(unsigned long port)
{
  return IsControl(port) && IsOutput(port);
}

bool CLadspaPlugin::IsAudioInput(unsigned long port)
{
  return IsAudio(port) && IsInput(port);
}

bool CLadspaPlugin::IsAudioOutput(unsigned long port)
{
  return IsAudio(port) && IsOutput(port);
}

bool CLadspaPlugin::HasLowerBound(unsigned long port)
{
  if (LADSPA_IS_HINT_BOUNDED_BELOW(PortRangeHint(port).HintDescriptor))
    return true;
  else
    return false;
}

bool CLadspaPlugin::HasUpperBound(unsigned long port)
{
  if (LADSPA_IS_HINT_BOUNDED_ABOVE(PortRangeHint(port).HintDescriptor))
    return true;
  else
    return false;
}

float CLadspaPlugin::LowerBound(unsigned long port, int samplerate /*= 48000*/)
{
  LADSPA_PortRangeHint hint = PortRangeHint(port);
  if (LADSPA_IS_HINT_SAMPLE_RATE(hint.HintDescriptor))
    return hint.LowerBound * (float)samplerate;
  else
    return hint.LowerBound;
}

float CLadspaPlugin::UpperBound(unsigned long port, int samplerate /*= 48000*/)
{
  LADSPA_PortRangeHint hint = PortRangeHint(port);
  if (LADSPA_IS_HINT_SAMPLE_RATE(hint.HintDescriptor))
    return hint.UpperBound * (float)samplerate;
  else
    return hint.UpperBound;
}

bool CLadspaPlugin::IsToggled(unsigned long port)
{
  if (LADSPA_IS_HINT_TOGGLED(PortRangeHint(port).HintDescriptor))
    return true;
  else
    return false;
}

bool CLadspaPlugin::IsLogarithmic(unsigned long port)
{
  if (LADSPA_IS_HINT_LOGARITHMIC(PortRangeHint(port).HintDescriptor))
    return true;
  else
    return false;
}

bool CLadspaPlugin::IsInteger(unsigned long port)
{
  if (LADSPA_IS_HINT_INTEGER(PortRangeHint(port).HintDescriptor))
    return true;
  else
    return false;
}

bool CLadspaPlugin::HasDefault(unsigned long port)
{
  if (LADSPA_IS_HINT_HAS_DEFAULT(PortRangeHint(port).HintDescriptor))
    return true;
  else
    return false;
}

float CLadspaPlugin::DefaultValue(unsigned long port, int samplerate /*= 48000*/)
{
  if (HasDefault(port))
  {
    LADSPA_PortRangeHintDescriptor hint = PortRangeHint(port).HintDescriptor;
    if (LADSPA_IS_HINT_DEFAULT_MINIMUM(hint))
      return LowerBound(port, samplerate);
    else if (LADSPA_IS_HINT_DEFAULT_LOW(hint))
      return MakeDefault(IsLogarithmic(port), LowerBound(port, samplerate), UpperBound(port, samplerate), 0.75f);
    else if (LADSPA_IS_HINT_DEFAULT_MIDDLE(hint))
      return MakeDefault(IsLogarithmic(port), LowerBound(port, samplerate), UpperBound(port, samplerate), 0.5f);
    else if (LADSPA_IS_HINT_DEFAULT_HIGH(hint))
      return MakeDefault(IsLogarithmic(port), LowerBound(port, samplerate), UpperBound(port, samplerate), 0.25f);
    else if (LADSPA_IS_HINT_DEFAULT_MAXIMUM(hint))
      return UpperBound(port, samplerate);
    else if (LADSPA_IS_HINT_DEFAULT_0(hint))
      return 0.0f;
    else if (LADSPA_IS_HINT_DEFAULT_1(hint))
      return 1.0f;
    else if (LADSPA_IS_HINT_DEFAULT_100(hint))
      return 100.0f;
    else if (LADSPA_IS_HINT_DEFAULT_440(hint))
      return 440.0f;
  }

  return 0.0f;
}

float CLadspaPlugin::MakeDefault(bool islog, float low, float high, float interpolate)
{
  if (islog)
    return exp(log(Max(low, 0.0000001)) * interpolate + log(Max(high, 0.0000001)) * (1.0f - interpolate));
  else
    return low * interpolate + high * (1.0f - interpolate);
}

