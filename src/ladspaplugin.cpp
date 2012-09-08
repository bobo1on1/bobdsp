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

bool CLadspaPlugin::SortByName(CLadspaPlugin* first, CLadspaPlugin* second)
{
  return string(first->Name()) < second->Name();
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
  for (unsigned long i = 0; i < m_descriptor->PortCount; i++)
  {
    if (LADSPA_IS_PORT_INPUT(m_descriptor->PortDescriptors[i]) &&
        LADSPA_IS_PORT_AUDIO(m_descriptor->PortDescriptors[i]))
      ports++;
  }

  return ports;
}

int CLadspaPlugin::AudioOutputPorts()
{
  int ports = 0;
  for (unsigned long i = 0; i < m_descriptor->PortCount; i++)
  {
    if (LADSPA_IS_PORT_OUTPUT(m_descriptor->PortDescriptors[i]) &&
        LADSPA_IS_PORT_AUDIO(m_descriptor->PortDescriptors[i]))
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

const char* CLadspaPlugin::DirectionStr(unsigned long port)
{
  if (LADSPA_IS_PORT_INPUT(PortDescriptor(port)))
    return "input";
  else if (LADSPA_IS_PORT_OUTPUT(PortDescriptor(port)))
    return "output";
  else
    return "unknown";
}

const char* CLadspaPlugin::TypeStr(unsigned long port)
{
  if (LADSPA_IS_PORT_CONTROL(PortDescriptor(port)))
    return "control";
  else if (LADSPA_IS_PORT_AUDIO(PortDescriptor(port)))
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

bool CLadspaPlugin::IsInput(unsigned long port)
{
  if (LADSPA_IS_PORT_INPUT(PortDescriptor(port)))
    return true;
  else
    return false;
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
    return exp(log(low) * interpolate + log(high) * (1.0f - interpolate));
  else
    return low * interpolate + high * (1.0f - interpolate);
}

