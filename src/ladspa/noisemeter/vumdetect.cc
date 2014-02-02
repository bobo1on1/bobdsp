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
#include "vumdetect.h"


int VUMdetect::init (int fsamp)
{
    reset ();
    _slow = false;
    _w = 10.6f / fsamp; 
    return 0;
}


void VUMdetect::reset (void)
{
    _z1 = _z2 = 0;
}


void VUMdetect::process (int n, const float *inp)
{
    float w, x, z1, z2;

    w = _slow ? (0.1f * _w) : _w;
    z1 = _z1 + 1e-30f;
    z2 = _z2;
    while (n--)
    {
	x = fabsf (*inp++) - 0.55f * z2;
	z1 += w * (x - z1);
	z2 += w * (z1 - z2);
    }
    if (z2 < 0) z2 = 0;
    _z1 = z1 - 1e-30f;
    _z2 = z2;
}
