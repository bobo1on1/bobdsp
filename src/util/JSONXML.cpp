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

//set an inner text value for the last added child
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

//going in one level deeper
int JSONXML::StartMap(void* ctx)
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
int JSONXML::MapKey(void* ctx, const unsigned char * key, YAJLSTRINGLEN stringLen)
{
  TiXmlElement*& element = *((TiXmlElement**)ctx);
  if (element)
    element->InsertEndChild(TiXmlElement(string((const char*)key, stringLen)));

  return 1;
}

//end of map, go up one level, set the pointer to the parent
int JSONXML::EndMap(void* ctx)
{
  TiXmlElement*& element = *((TiXmlElement**)ctx);
  if (element && element->Parent())
    element = element->Parent()->ToElement();

  return 1;
}

//start of array, MapKey should have been called before this to allocate a new element
int JSONXML::StartArray(void* ctx)
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

int JSONXML::EndArray(void* ctx)
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

