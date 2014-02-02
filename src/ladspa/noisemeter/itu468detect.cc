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
#include "itu468detect.h"


int Itu468detect::init (int fsamp)
{
    reset ();

    _a1 = 670.0f / fsamp;
    _b1 = 3.50f / fsamp;
    _a2 = 6.60f / fsamp;
    _b2 = 0.65f / fsamp;

    return 0;
}


void Itu468detect::reset (void)
{
    _z1 = _z2 = 0;
}


void Itu468detect::process (int n, const float *inp)
{
    float x, z1, z2;

    z1 = _z1;
    z2 = _z2;

    while (n--)
    {
	x = fabsf (*inp++) + 1e-30f;
	z1 -= z1 * _b1;
	if (x > z1) z1 += _a1 * (x - z1);
	z2 -= z2 * _b2;
	if (z1 > z2) z2 += _a2 * (z1 - z2);
    }

    _z1 = z1;
    _z2 = z2;
}
