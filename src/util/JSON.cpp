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

#include "JSON.h"
#include "util/misc.h"

using namespace std;

namespace JSON
{
  int Boolean(void* ctx, int boolVal);
  int Number(void* ctx, const char * numberVal, YAJLSTRINGLEN numberLen);
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
  JSON::Boolean,
  NULL,
  NULL,
  JSON::Number,
  JSON::String,
  JSON::StartMap,
  JSON::MapKey,
  JSON::EndMap,
  JSON::StartArray,
  JSON::EndArray,
};

TiXmlElement* JSON::JSONToXML(const std::string& json)
{
  //allocate a root element to work with
  TiXmlElement* root = new TiXmlElement("JSON");
  //pointer for yajl to work with
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

int JSON::Boolean(void* ctx, int boolVal)
{
  string boolean = ToString(boolVal ? true : false);
  return JSON::String(ctx, (const unsigned char*)boolean.c_str(), boolean.length());
}

int JSON::Number(void* ctx, const char * numberVal, YAJLSTRINGLEN numberLen)
{
  return JSON::String(ctx, (const unsigned char*)numberVal, numberLen);
}

//set an inner text value for the last added child
int JSON::String(void* ctx, const unsigned char * stringVal, YAJLSTRINGLEN stringLen)
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

//going in one level deeper
int JSON::StartMap(void* ctx)
{
  TiXmlElement*& element = *((TiXmlElement**)ctx);
  if (element)
  {
    TiXmlNode* child = element->LastChild();
    if (child)
    {
      int userdata = (long)child->GetUserData();
      if (userdata == 1) //first element of array
      {
        element = child->ToElement(); //set the pointer to the child
        element->SetUserData((void*)2); //after this, a new child element should be added
      }
      if (userdata == 2) //add a new element to the array, with the same name as the previous element
      {
        element = element->InsertEndChild(TiXmlElement(child->Value()))->ToElement();
        element->SetUserData((void*)2);
      }
      else //standard element, set the pointer to the last added child
      {
        element = child->ToElement();
      }
    }
  }

  return 1;
}

//new key, add a new child element
int JSON::MapKey(void* ctx, const unsigned char * key, YAJLSTRINGLEN stringLen)
{
  TiXmlElement*& element = *((TiXmlElement**)ctx);
  if (element)
    element->InsertEndChild(TiXmlElement(string((const char*)key, stringLen)));

  return 1;
}

//end of map, go up one level, set the pointer to the parent
int JSON::EndMap(void* ctx)
{
  TiXmlElement*& element = *((TiXmlElement**)ctx);
  if (element && element->Parent())
    element = element->Parent()->ToElement();

  return 1;
}

//start of array, MapKey should have been called before this to allocate a new element
int JSON::StartArray(void* ctx)
{
  TiXmlElement*& element = *((TiXmlElement**)ctx);
  if (element)
  {
    TiXmlNode* child = element->LastChild();
    if (child)
      child->SetUserData((void*)1); //tell StartMap that this is an array
  }

  return 1;
}

int JSON::EndArray(void* ctx)
{
  TiXmlElement*& element = *((TiXmlElement**)ctx);
  if (element)
  {
    TiXmlNode* child = element->LastChild();
    if (child)
    {
      if ((long)child->GetUserData() == 1)
        element->RemoveChild(child); //array with 0 elements, remove
      else
        child->SetUserData((void*)0); //tell StartMap that the array has ended
    }
  }

  return 1;
}

JSON::CJSONGenerator::CJSONGenerator()
{
#if YAJL_MAJOR == 2
  m_handle = yajl_gen_alloc(NULL);
  yajl_gen_config(m_handle, yajl_gen_beautify, 1);
  yajl_gen_config(m_handle, yajl_gen_indent_string, "  ");
#else
  yajl_gen_config yajlconfig;
  yajlconfig.beautify = 1;
  yajlconfig.indentString = "  ";
  m_handle = yajl_gen_alloc(&yajlconfig, NULL);
#endif
}

JSON::CJSONGenerator::~CJSONGenerator()
{
  yajl_gen_clear(m_handle);
  yajl_gen_free(m_handle);
}

void JSON::CJSONGenerator::Reset()
{
  yajl_gen_clear(m_handle);
}

std::string JSON::CJSONGenerator::ToString()
{
  const unsigned char* str;
  YAJLSTRINGLEN length;
  yajl_gen_get_buf(m_handle, &str, &length);
  return string((const char *)str, length);
}

void JSON::CJSONGenerator::AddInt(int64_t in)
{
  string number = ::ToString(in);
  yajl_gen_number(m_handle, number.c_str(), number.length());
}

void JSON::CJSONGenerator::AddDouble(double in)
{
  string number = ::ToString(in);
  yajl_gen_number(m_handle, number.c_str(), number.length());
}

