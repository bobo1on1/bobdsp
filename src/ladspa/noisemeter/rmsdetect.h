// ----------------------------------------------------------------------- 
//
//  Copyright (C) 2010 Fons Adriaensen <fons@kokkinizita.net>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
// ----------------------------------------------------------------------- 


#ifndef __RMSDETECT_H
#define __RMSDETECT_H


#include <math.h>


class RMSdetect
{
public:

    RMSdetect (void) {}
    ~RMSdetect (void) {}

    int   init (int fsamp);
    void  reset (void);
    void  speed (bool slow) { _slow = slow; }
    void  process (int n, const float *inp);
    float value (void) { return sqrtf (2 * _z); }

private:

    bool  _slow;
    float _w;
    float _z;
};


#endif


