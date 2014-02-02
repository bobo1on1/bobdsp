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


#ifndef __ITU468FILTER_H
#define __ITU468FILTER_H


#include <stdlib.h>


class Itu468filter
{
public:

    Itu468filter (void) : _err (true) {}
    ~Itu468filter (void) {}

    int  init (int fsamp, bool ref2k = false);
    void reset (void);
    void process (size_t n, const float *inp, float *out);

private:

    bool     _err;
    float    _whp;
    float    _a11, _a12;
    float    _a21, _a22;
    float    _a31, _a32;
    float    _b30, _b31, _b32;
    float    _zhp;
    float    _z11, _z12;
    float    _z21, _z22;
    float    _z31, _z32;
};

#endif


