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
#include <string.h>
#include "acfilter.h"


#define ACW_F1  20.5990
#define ACW_F2  107.652
#define ACW_F3  737.862
#define ACW_F4  12194.2
#define AW_GAIN 1.257
#define CW_GAIN 1.006


int ACfilter::init (int fsamp)
{
    double f, g;

    reset ();
    _w1 = _w2 = _w3 = _w4 = _ga = _gc = 0;     

    _err = false;
    switch (fsamp)
    {
    case 44100:	_w4 = 0.846; break;
    case 48000:	_w4 = 0.817; break;
    case 88200:	_w4 = 0.587; break;
    case 96000:	_w4 = 0.555; break;
    default:
	_err = true;
        return 1;
    }

    f = ACW_F1 / fsamp;
    _w1 = 2 * M_PI * f;
    g = 4 / ((2 - _w1) * (2 - _w1));
    _w1 *= 1 - 3 * f;
    _gc = CW_GAIN * g;

    f = ACW_F2 / fsamp;
    _w2 = 2 * M_PI * f;
    g *= 2 / (2 - _w2);
    _w2 *= 1 - 3 * f;

    f = ACW_F3 / fsamp;
    _w3 = 2 * M_PI * f;
    g *= 2 / (2 - _w3);
    _w3 *= 1 - 3 * f;
    _ga = AW_GAIN * g;

    return 0;
}


void ACfilter::reset (void)
{
    // reset filter state
    _z1a = _z1b = _z2 = _z3 = _z4a = _z4b = 0;
}


void ACfilter::process (size_t n, const float *in, float *opA, float *opC)
{
    float x, e;

    if (_err)
    {
	if (opA) memset (opA, 0, n * sizeof (float));
	if (opC) memset (opC, 0, n * sizeof (float));
        return;
    }

    e = 1e-20f;
    while (n--)
    {
	x = *in++;
        // highpass sections, A and C
        _z1a += _w1 * (x - _z1a + e); 
        x -= _z1a;
        _z1b += _w1 * (x - _z1b + e); 
        x -= _z1b;
        // lowpass sections, A, and C   
        _z4a += _w4 * (x - _z4a);
        x  = 0.25f * _z4b;
        _z4b += _w4 * (_z4a - _z4b);
        x += 0.75f * _z4b;
        if (opC) *opC++ = _gc * x;
        // highpass sections, A only
        _z2 += _w2 * (x - _z2 + e); 
        x -= _z2;
        _z3 += _w3 * (x - _z3 + e); 
        x -= _z3;
        if (opA) *opA++ = _ga * x;
    }
}
