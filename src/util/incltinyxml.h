/*
 * bobdsp
 * Copyright (C) Bob 2012
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

#ifndef INCLTINYXML_H
#define INCLTINYXML_H

//compile fix for TinyXml < 2.6.0
#define DOCUMENT    TINYXML_DOCUMENT
#define ELEMENT     TINYXML_ELEMENT
#define COMMENT     TINYXML_COMMENT
#define UNKNOWN     TINYXML_UNKNOWN
#define TEXT        TINYXML_TEXT
#define DECLARATION TINYXML_DECLARATION
#define TYPECOUNT   TINYXML_TYPECOUNT

#include <tinyxml.h>

#undef DOCUMENT
#undef ELEMENT
#undef COMMENT
#undef UNKNOWN
#undef TEXT
#undef DECLARATION
#undef TYPECOUNT

#endif //INCLTINYXML_H
