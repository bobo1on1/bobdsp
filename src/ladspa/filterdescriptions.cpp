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

#include <stddef.h>
#include "config.h"
#include "filterdescriptions.h"
#include "filterinterface.h"

using namespace BobDSPLadspa;

#define FUNCTIONPTRS\
  Instantiate,\
  ConnectPort,\
  Activate,\
  Run,\
  NULL,\
  NULL,\
  Deactivate,\
  Cleanup,

const LADSPA_Descriptor CFilterDescriptions::m_descriptors[] =
{
  {
    LINKWITZTRANSFORM,
    "bobdsplt",
    LADSPA_PROPERTY_HARD_RT_CAPABLE,
    "BobDSP Linkwitz transform",
    "Bob",
    "GPLv3",
    6,
    (const int[])
    {
      LADSPA_PORT_INPUT  | LADSPA_PORT_AUDIO,
      LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO,
      LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
      LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
      LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
      LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL
    },
    (const char*[])
    {
      "Input",
      "Output",
      "f(0)",
      "Q(0)",
      "f(p)",
      "Q(p)"
    },
    (const LADSPA_PortRangeHint[])
    {
      {},
      {},
      {
        LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE |
        LADSPA_HINT_LOGARITHMIC | LADSPA_HINT_DEFAULT_100,
        0.0f,
        200.0f
      },
      {
        LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_1 |
        LADSPA_HINT_LOGARITHMIC,
        0.01f,
        5.0f
      },
      {
        LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE |
        LADSPA_HINT_LOGARITHMIC | LADSPA_HINT_DEFAULT_100,
        0.0f,
        200.0f
      },
      {
        LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_1 |
        LADSPA_HINT_LOGARITHMIC,
        0.01f,
        5.0f
      }
    },
    NULL,
    FUNCTIONPTRS
  },
  {
    DITHER,
    "bobdspdither",
    LADSPA_PROPERTY_HARD_RT_CAPABLE,
    "BobDSP Dither",
    "Bob",
    "GPLv3",
    3,
    (const int[])
    {
      LADSPA_PORT_INPUT  | LADSPA_PORT_AUDIO,
      LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO,
      LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
    },
    (const char*[])
    {
      "Input",
      "Output",
      "Bit depth",
    },
    (const LADSPA_PortRangeHint[])
    {
      {},
      {},
      {
        LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE |
        LADSPA_HINT_INTEGER | LADSPA_HINT_DEFAULT_MIDDLE,
        0.0f,
        32.0f
      }
    },
    NULL,
    FUNCTIONPTRS
  },
#ifdef USE_SPEEX
  {
    ECHOCANCELLATION,
    "speexechocancellation",
    0,
    "Speex echo cancellation",
    "Bob",
    "GPLv3",
    4,
    (const int[])
    {
      LADSPA_PORT_INPUT  | LADSPA_PORT_AUDIO,
      LADSPA_PORT_INPUT  | LADSPA_PORT_AUDIO,
      LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO,
      LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL
    },
    (const char*[])
    {
      "Input",
      "Echo",
      "Output",
      "Filter length"
    },
    (const LADSPA_PortRangeHint[])
    {
      {},
      {},
      {},
      {
        LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE |
        LADSPA_HINT_DEFAULT_MIDDLE,
        0.0f,
        1.0f
      }
    },
    NULL,
    FUNCTIONPTRS
  }
#endif //USE_SPEEX
};

const LADSPA_Descriptor* CFilterDescriptions::Descriptor(unsigned long index)
{
  if (index < NrDescriptors())
    return m_descriptors + index;
  else
    return NULL;
}

unsigned long CFilterDescriptions::NrDescriptors()
{
  return sizeof(m_descriptors) / sizeof(m_descriptors[0]);
}
