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

#include <math.h>
#include "biquadcoefs.h"
#include "util/misc.h"

using namespace BobDSPLadspa;

CBiquadCoef::CBiquadCoef()
{
  a0 = 1.0f;
  a1 = 0.0f;
  a2 = 0.0f;
  b0 = 1.0f;
  b1 = 0.0f;
  b2 = 0.0f;

  for (size_t i = 0; i < sizeof(m_oldsettings) / sizeof(m_oldsettings[0]); i++)
    m_oldsettings[i] = -1.0f;
}

CBiquadCoef::~CBiquadCoef()
{
  Passthrough();
}

void CBiquadCoef::Calculate(EFILTER type, float samplerate, LADSPA_Data** ports, bool force)
{
  if (type == LINKWITZTRANSFORM)
    LinkwitzTransform(samplerate, ports, force);
  else
    Passthrough();
}

//fallback filter, output = input
void CBiquadCoef::Passthrough()
{
  a0 = 1.0f;
  a1 = 0.0f;
  a2 = 0.0f;
  b0 = 1.0f;
  b1 = 0.0f;
  b2 = 0.0f;
}

//ported from the spreadsheet at http://www.minidsp.com/applications/advanced-tools/linkwitz-transform
//I don't understand what's going on here, so I can't explain what it's doing

void CBiquadCoef::LinkwitzTransform(float samplerate, LADSPA_Data** ports, bool force)
{
  /*
    B3 = f0
    B4 = q0
    B5 = fp
    B6 = qp
    B10 = samplerate

    a1 =-(2*(B30-(B34^2)*B32)/B35)
    a2 =-((B30-B34*B31+(B34^2)*B32)/B35)
    b0 =($B$26+$B$34*$B$27+($B$34^2)*$B$28)/$B$35
    b1 =2*($B$26-($B$34^2)*$B$28)/$B$35
    b2 =($B$26-$B$34*$B$27+($B$34^2)*$B$28)/$B$35

    B11 =AVERAGE(B3,B5)
    B26 =(2*PI()*B3)^2
    B27 =(2*PI()*B3)/B4
    B28 = B9 = 1
    B30 =(2*PI()*B5)^2
    B31 =(2*PI()*B5)/B6
    B32 = B9 = 1
    B34 =(2*PI()*B11)/(TAN(PI()*B11/B10))
    B35 =B30+B34*B31+(B34^2)*B32
  
    */

  //only calculate the coefficients if the parameters changed,
  //or when forced to calculate them
  bool changed = false;
  for (int i = 0; i < 4; i++)
  {
    if (m_oldsettings[i] != *ports[i + 2])
    {
      changed = true;
      m_oldsettings[i] = *ports[i + 2];
    }
  }

  if (!changed && !force)
    return;

  //clamp frequencies and Q factors to something sane
  LADSPA_Data f0 = Clamp(*ports[2], 1.0f, samplerate * 0.4f);
  LADSPA_Data q0 = Clamp(*ports[3], 0.001f, 50.0f);
  LADSPA_Data fp = Clamp(*ports[4], 1.0f, samplerate * 0.4f);
  LADSPA_Data qp = Clamp(*ports[5], 0.001f, 50.0f);

  LADSPA_Data B11 = (f0 + fp) / 2.0f;
  LADSPA_Data B26 = powf(2.0f * M_PI * f0, 2.0f);
  LADSPA_Data B27 = (2.0f * M_PI * f0) / q0;
  LADSPA_Data B30 = powf(2.0f * M_PI * fp, 2.0f);
  LADSPA_Data B31 = (2.0f * M_PI * fp) / qp;
  LADSPA_Data B34 = (2.0f * M_PI * B11) / tanf(M_PI * B11 / samplerate);
  LADSPA_Data B35 = B30 + (B34 * B31) + powf(B34, 2.0f);

  a0 = 1.0f;
  a1 = -(2.0f * (B30 - powf(B34, 2.0f)) / B35);
  a2 = -((B30 - B34 * B31 + powf(B34, 2.0f)) / B35);

  b0 = (B26 + B34 * B27 + powf(B34, 2.0f)) / B35;
  b1 = 2.0f * (B26 - powf(B34, 2.0f)) / B35;
  b2 = (B26 - B34 * B27 + powf(B34, 2.0f)) / B35;
}

