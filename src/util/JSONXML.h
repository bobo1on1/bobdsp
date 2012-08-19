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

#ifndef JSONXML_H
#define JSONXML_H

#include "util/incltinyxml.h"
#include <string>

#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>
#ifdef HAVE_YAJL_YAJL_VERSION_H
#include <yajl/yajl_version.h>
#endif

#if YAJL_MAJOR == 2
  #define YAJLSTRINGLEN size_t
#else
  #define YAJLSTRINGLEN unsigned int
#endif

namespace JSONXML
{
  TiXmlElement* JSONToXML(const std::string& json);
}

#endif //JSONXML_H
