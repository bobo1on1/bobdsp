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

#include "JSONXML.h"

using namespace std;

namespace JSONXML
{
  int String(void* ctx, const unsigned char * stringVal, YAJLSTRINGLEN stringLen);
  int StartMap(void* ctx);
  int MapKey(void* ctx, const unsigned char * key, YAJLSTRINGLEN stringLen);
  int EndMap(void* ctx);
  int StartArray(void* ctx);
  int EndArray(void* ctx);
}

static yajl_callbacks callbacks =
{
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  JSONXML::String,
  JSONXML::StartMap,
  JSONXML::MapKey,
  JSONXML::EndMap,
  JSONXML::StartArray,
  JSONXML::EndArray,
};

TiXmlElement* JSONXML::JSONToXML(const std::string& json)
{
  TiXmlElement* root = new TiXmlElement("JSON");
  TiXmlElement* rootptr = root;

  yajl_handle handle;

#if YAJL_MAJOR == 2
  handle = yajl_alloc(&callbacks, NULL, &rootptr);
  yajl_config(handle, yajl_allow_comments, 1);
  yajl_config(handle, yajl_dont_validate_strings, 0);
#else
  yajl_parser_config yajlconfig;
  yajlconfig.allowComments = 1;
  yajlconfig.checkUTF8 = 1;
  handle = yajl_alloc(&callbacks, &yajlconfig, NULL, &rootptr);
#endif

  yajl_parse(handle, (const unsigned char*)json.c_str(), json.length());

#if YAJL_MAJOR == 2
  yajl_complete_parse(handle);
#else
  yajl_parse_complete(handle);
#endif

  yajl_free(handle);

  return root;
}

int JSONXML::String(void* ctx, const unsigned char * stringVal, YAJLSTRINGLEN stringLen)
{
  TiXmlElement*& element = *((TiXmlElement**)ctx);
  if (element)
  {
    TiXmlNode* child = element->LastChild();
    if (child)
    {
      TiXmlText value(string((const char*)stringVal, stringLen));
      child->ToElement()->InsertEndChild(value);
    }
  }

  return 1;
}

int JSONXML::StartMap(void* ctx)
{
  TiXmlElement*& element = *((TiXmlElement**)ctx);
  if (element)
  {
    TiXmlNode* child = element->LastChild();
    if (child)
    {
      int userdata = (int)child->GetUserData();
      if (userdata == 1)
      {
        element = child->ToElement();
        element->SetUserData((void*)2);
      }
      if (userdata == 2)
      {
        element = element->InsertEndChild(TiXmlElement(child->Value()))->ToElement();
        element->SetUserData((void*)2);
      }
      else
      {
        element = child->ToElement();
      }
    }
  }

  return 1;
}

int JSONXML::MapKey(void* ctx, const unsigned char * key, YAJLSTRINGLEN stringLen)
{
  TiXmlElement*& element = *((TiXmlElement**)ctx);
  if (element)
    element->InsertEndChild(TiXmlElement(string((const char*)key, 0, stringLen)));

  return 1;
}

int JSONXML::EndMap(void* ctx)
{
  TiXmlElement*& element = *((TiXmlElement**)ctx);
  if (element && element->Parent())
    element = element->Parent()->ToElement();

  return 1;
}

int JSONXML::StartArray(void* ctx)
{
  TiXmlElement*& element = *((TiXmlElement**)ctx);
  if (element)
  {
    TiXmlNode* child = element->LastChild();
    if (child)
      child->SetUserData((void*)1);
  }

  return 1;
}

int JSONXML::EndArray(void* ctx)
{
  TiXmlElement*& element = *((TiXmlElement**)ctx);
  if (element)
  {
    TiXmlNode* child = element->LastChild();
    if (child)
      child->SetUserData((void*)0);
  }

  return 1;
}

