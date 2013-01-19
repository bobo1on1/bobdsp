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

#ifndef JSON_H
#define JSON_H

#include "config.h"
#include "util/inclstdint.h"
#include "util/incltinyxml.h"
#include <string>
#include <cstring>

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

namespace JSON
{
  TiXmlElement* JSONToXML(const std::string& json);

  class CJSONGenerator
  {
    public:
      CJSONGenerator();
      ~CJSONGenerator();

      void        Reset();
      std::string ToString();
      void        ToString(std::string& jsonstr);
      void        AppendToString(std::string& jsonstr);

      void MapOpen()    { yajl_gen_map_open(m_handle);    }
      void MapClose()   { yajl_gen_map_close(m_handle);   }
      void ArrayOpen()  { yajl_gen_array_open(m_handle);  }
      void ArrayClose() { yajl_gen_array_close(m_handle); }
      void AddString(const std::string& in)
        { yajl_gen_string(m_handle, (const unsigned char*)in.c_str(), in.length()); }
      void AddString(const char* in)
        { yajl_gen_string(m_handle, (const unsigned char*)in, strlen(in)); }
      void AddInt(int64_t in);
      void AddDouble(double in);
      void AddBool(bool in)
        { yajl_gen_bool(m_handle, in); }

    private:
      yajl_gen m_handle;
  };
}

#endif //JSON_H
