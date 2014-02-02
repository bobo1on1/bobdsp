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


#include "lpeq20filter.h"

#include <stdio.h>


//float calcpar (float f)
//{
//    if      (f <= 0.5f) return 4.443f * f / (1 - f * (1.2f - f * (13.6f - f * 10.5f)));
//    else if (f <= 1.0f) return 0.786130f + f * 0.213870f;
//    else return 1;
//}


int LPeq20filter::init (int fsamp)
{
    reset ();
    switch (fsamp)
    {
    case 44100:
	_g  =  6.17251745e-01f;
	_b1 =  3.03653041e+00f;
	_b2 =  3.55941563e+00f;
	_b3 =  1.89264513e+00f;
	_b4 =  3.87436747e-01f;
	break;
    case 48000:
	_g  =  4.27293435e-01f;
	_b1 =  2.32683301e+00f;
	_b2 =  2.28195320e+00f;
	_b3 =  1.03148006e+00f;
	_b4 =  1.96428697e-01f;
	break;
    case 88200:
	_g  =  4.26385390e-02f;
	_b1 = -1.02651917e+00f;
	_b2 =  1.07245896e+00f;
	_b3 = -4.86158787e-01f;
	_b4 =  1.22435626e-01f;
	break;
    case 96000:
	_g  =  3.14009927e-02f;
	_b1 = -1.32061860e+00f;
	_b2 =  1.29625957e+00f;
	_b3 = -6.18938600e-01f;
	_b4 =  1.45713514e-01f;
        break;
    default:
        return 1;
    }
    return 0;
}


void LPeq20filter::reset (void)
{
    _z1 = _z2 = _z3 = _z4 = 0;
}


void LPeq20filter::process (size_t n, const float *inp, float *out)
{
    float x, z1, z2, z3, z4;

    z1 = _z1;
    z2 = _z2;
    z3 = _z3;
    z4 = _z4;
    while (n--)
    {
	x = *inp++ + 1e-20f;
	x -= _b1 * z1 + _b2 * z2 + _b3 * z3 + _b4 * z4;
	*out++ = _g * (x + z4 + 4 * (z1 + z3) + 6 * z2);
	z4 = z3;
	z3 = z2;
	z2 = z1;
	z1 = x;
    }
    _z1 = z1;
    _z2 = z2;
    _z3 = z3;
    _z4 = z4;
}

