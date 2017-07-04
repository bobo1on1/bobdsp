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
#ifndef FLOATBUFFEROPS_H
#define FLOATBUFFEROPS_H

namespace UTILNAMESPACE
{
  void ApplyGain(float* data, int samples, float gain);
  void CopyApplyGain(float* in, float* out, int samples, float gain);
  void DenormalsToZero(float* data, int samples);
  void AvgSquare(float* data, int samples, float& avg);
  void AvgAbs(float* data, int samples, float& avg);
  void HighestAbs(float* data, int samples, float& value);
}

using namespace UTILNAMESPACE;

#endif //FLOATBUFFEROPS_H
