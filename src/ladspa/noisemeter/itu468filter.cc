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


#include <string.h>
#include "itu468filter.h"


#define GREF2K 0.5239f


int Itu468filter::init (int fsamp, bool ref2k)
{
    reset ();

    _err = false;
    switch (fsamp)
    {
    case 44100:
	_whp =  4.1330773e-01f;
	_a11 = -7.3360199e-01f;
	_a12 =  2.5954875e-01f;
	_a21 = -6.1104256e-01f;
	_a22 =  2.3008855e-01f;
	_a31 = -1.8076769e-01f;
	_a32 =  4.0974531e-01f;
	_b30 =  1.3153632e+00f;
	_b31 =  7.7909422e-01f;
	_b32 = -8.1194239e-02f;
        break;

    case 48000:
	_whp =  3.8715217e-01f;
	_a11 = -8.4163201e-01f;
	_a12 =  3.0498350e-01f;
	_a21 = -6.5680242e-01f;
	_a22 =  2.3733993e-01f;
	_a31 = -3.3843556e-01f;
	_a32 =  4.3756709e-01f;
	_b30 =  9.8607997e-01f;
	_b31 =  5.4846389e-01f;
	_b32 = -8.2465158e-02f;
  	break;

    case 88200:
	_whp =  2.4577479e-01f;
	_a11 = -1.3820207e+00f;
	_a12 =  5.6534863e-01f;
	_a21 = -9.7786880e-01f;
	_a22 =  2.8603959e-01f;
	_a31 = -1.2184392e+00f;
	_a32 =  6.4096606e-01f;
	_b30 =  9.5345587e-02f;
	_b31 =  3.6653187e-02f;
	_b32 = -2.0960915e-02f;
   	break;

    case 96000:
	_whp =  2.2865345e-01f;
	_a11 = -1.4324744e+00f;
	_a12 =  5.9176731e-01f;
	_a21 = -1.0594915e+00f;
	_a22 =  3.2190937e-01f;
	_a31 = -1.2991971e+00f;
	_a32 =  6.6485137e-01f;
	_b30 =  6.7263212e-02f;
	_b31 =  2.1102539e-02f;
	_b32 = -1.7972740e-02f;
  	break;

    default:
	_err = true;
        return 1;
    }

    if (ref2k)
    {
	_b30 *= GREF2K;
	_b31 *= GREF2K;
	_b32 *= GREF2K;
    }

    return 0;
}


void Itu468filter::reset (void)
{
    _zhp = 0;
    _z11 = _z12 = 0;
    _z21 = _z22 = 0;
    _z31 = _z32 = 0;
}


void Itu468filter::process (size_t n, const float *inp, float *out)
{
    float x, zhp, z11, z12, z21, z22, z31, z32;

    if (_err)
    {
	memset (out, 0, n * sizeof (float));
        return;
    }

    zhp = _zhp;
    z11 = _z11;
    z12 = _z12;
    z21 = _z21;
    z22 = _z22;
    z31 = _z31;
    z32 = _z32;

    while (n--)
    {
	x = *inp++;
	zhp += _whp * (x - zhp) + 1e-20f;
	x -= zhp;
	x -= _a11 * z11 + _a12 * z12;
	z12 = z11;
	z11 = x;
	x -= _a21 * z21 + _a22 * z22;
	z22 = z21;
	z21 = x;
	x -= _a31 * z31 + _a32 * z32;
	*out++ = _b30 * x + _b31 * z31 + _b32 * z32;
	z32 = z31;
	z31 = x;
    }

    _zhp = zhp;
    _z11 = z11;
    _z12 = z12;
    _z21 = z21;
    _z22 = z22;
    _z31 = z31;
    _z32 = z32;
}
