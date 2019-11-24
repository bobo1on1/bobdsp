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

#include <ladspa.h>
#include <stddef.h>
#include "config.h"
#include "biquad.h"
#include "dither.h"
#include "noisemeterweighting.h"
#include "noisemeterdetect.h"
#include "switch.h"
#include "pwm.h"
#include "dpl2encoder.h"
#include "hilberttransformplugin.h"
#include "distancedelay.h"
#include "filterdescriptions.h"
#include "filterinterface.h"

#ifdef USE_SPEEX
  #include "echocancellation.h"
#endif

using namespace BobDSPLadspa;

extern "C"
{
  const LADSPA_Descriptor* ladspa_descriptor(unsigned long Index)
  {
    const LADSPA_Descriptor* descriptor = CFilterDescriptions::Descriptor(Index);
    return descriptor;
  }
}

LADSPA_Handle BobDSPLadspa::Instantiate(const struct _LADSPA_Descriptor* Descriptor, unsigned long samplerate)
{
  if (Descriptor->UniqueID == LINKWITZTRANSFORM)
    return new CBiquad((EFILTER)Descriptor->UniqueID, samplerate);
  else if (Descriptor->UniqueID == DITHER)
    return new CDither();
#ifdef USE_SPEEX
  else if (Descriptor->UniqueID == ECHOCANCELLATION)
    return new CEchoCancellation(samplerate);
#endif
  else if (Descriptor->UniqueID == NOISEMETERWEIGHTING)
    return new CNoiseMeterWeighting(samplerate);
  else if (Descriptor->UniqueID == NOISEMETERDETECT)
    return new CNoiseMeterDetect(samplerate);
  else if (Descriptor->UniqueID == SWITCH)
    return new CSwitch(samplerate);
  else if (Descriptor->UniqueID == PWM)
    return new CPwm(samplerate);
  else if (Descriptor->UniqueID == DPL2ENCODER)
    return new CDPL2Encoder(samplerate);
  else if (Descriptor->UniqueID == HILBERTTRANSFORM)
    return new CHilbertTransformPlugin();
  else if (Descriptor->UniqueID == DISTANCEDELAY)
    return new CDistanceDelay(samplerate);
  else
    return NULL;
}

void BobDSPLadspa::ConnectPort(LADSPA_Handle instance, unsigned long port, LADSPA_Data* datalocation)
{
  ((IFilter*)instance)->ConnectPort(port, datalocation);
}

void BobDSPLadspa::Activate(LADSPA_Handle instance)
{
  ((IFilter*)instance)->Activate();
}

void BobDSPLadspa::Run(LADSPA_Handle instance, unsigned long samplecount)
{
  ((IFilter*)instance)->Run(samplecount);
}

void BobDSPLadspa::Deactivate(LADSPA_Handle instance)
{
  ((IFilter*)instance)->Deactivate();
}

void BobDSPLadspa::Cleanup(LADSPA_Handle instance)
{
  delete ((IFilter*)instance);
}

