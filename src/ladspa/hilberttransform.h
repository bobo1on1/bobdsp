/*
 * bobdsp
 * Copyright (C) Bob 2017
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

#ifndef HILBERTTRANSFORM_H
#define HILBERTTRANSFORM_H

#define FILTERSIZE 512
#define BUFSIZE (FILTERSIZE / 2)

namespace BobDSPLadspa
{
  class CHilbertTransform
  {
    public:
      CHilbertTransform();
      ~CHilbertTransform();
      void  Reset();
      float Process(float in);

    private:
      int          m_bufindex;
      float*       m_buf[2];
  };
}

#endif //HILBERTTRANSFORM_H
