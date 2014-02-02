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


#include <math.h>
#include "rmsdetect.h"


int RMSdetect::init (int fsamp)
{
    reset ();
    _slow = false;
    _w = 8.0f / fsamp; 
    return 0;
}


void RMSdetect::reset (void)
{
    _z = 0;
}


void RMSdetect::process (int n, const float *inp)
{
    float w, x, z;

    w = _slow ? (_w / 8) : _w;
    z = _z + 1e-30f;
    while (n--)
    {
	x = *inp++;
	z += w * (x * x - z);
    }
    _z = z;
}

