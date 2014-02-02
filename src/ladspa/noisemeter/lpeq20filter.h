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


#ifndef __LPEQ20FILTER_H
#define __LPEQ20FILTER_H


#include <stdlib.h>


class LPeq20filter
{
public:

    LPeq20filter (void) {}
    ~LPeq20filter (void) {}

    int  init (int fsamp);
    void reset (void);
    void process (size_t n, const float *inp, float *out);

private:

    float _g;
    float _b1, _b2, _b3, _b4;	       
    float _z1, _z2, _z3, _z4;
};

#endif


