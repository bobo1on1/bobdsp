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

#define TIXML_USE_STL

#include <tinyxml.h>

#undef DOCUMENT
#undef ELEMENT
#undef COMMENT
#undef UNKNOWN
#undef TEXT
#undef DECLARATION
#undef TYPECOUNT

//so, I tried to make something to load elements from xml, without having to enter the element name several times
//and it turned into this ugly pile of macros, meh it works
#define LOADELEMENT(element, name, mandatory) TiXmlElement* name = element->FirstChildElement(#name);\
bool name ## _loadfailed = false;\
if (!name)\
{\
  name ## _loadfailed = true;\
  (void) name ## _loadfailed ;\
  if (mandatory)\
  {\
    LogError("<" #name "> element missing");\
    loadfailed = true;\
  }\
}\
else if (!name->GetText() || strlen(name->GetText()) == 0)\
{\
  name ## _loadfailed = true;\
  LogError("<" #name "> element empty");\
  if (mandatory)\
    loadfailed = true;\
}\

#define PARSEELEMENT(element, name, mandatory, default, type, parsefunc, postcheck)\
LOADELEMENT(element, name, mandatory);\
type name ## _p = default;\
bool name ## _parsefailed = false;\
(void) name ## _parsefailed;\
if (!name ## _loadfailed)\
{\
  if (!parsefunc(name->GetText(), name ## _p) || !postcheck(name ## _p))\
  {\
    LogError("Invalid value %s for element <" #name ">", name->GetText());\
    name ## _parsefailed = true;\
    name ## _p = default;\
    if (mandatory)\
      loadfailed = true;\
  }\
}

#define POSTCHECK_NONE(value) (true)
#define POSTCHECK_ONEORHIGHER(value) (value >= 1)

#define MANDATORY true
#define OPTIONAL  false

#define LOADFLOATELEMENT(element, name, mandatory, default, postcheck)\
PARSEELEMENT(element, name, mandatory, default, double, StrToFloat, postcheck);

#define LOADINTELEMENT(element, name, mandatory, default, postcheck)\
PARSEELEMENT(element, name, mandatory, default, int64_t, StrToInt, postcheck);

#endif //INCLTINYXML_H
