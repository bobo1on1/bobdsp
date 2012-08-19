/*
 * boblight
 * Copyright (C) Bob  2009 
 * 
 * boblight is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * boblight is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <locale.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include "misc.h"

using namespace std;

void PrintError(const std::string& error)
{
  std::cerr << "ERROR: " << error << "\n";
}

//get the first word (separated by whitespace) from string data and place that in word
//then remove that word from string data
bool GetWord(string& data, string& word)
{
  stringstream datastream(data);
  string end;

  datastream >> word;
  if (datastream.fail())
  {
    data.clear();
    return false;
  }

  size_t pos = data.find(word) + word.length();

  if (pos >= data.length())
  {
    data.clear();
    return true;
  }

  data = data.substr(pos);
  
  datastream.clear();
  datastream.str(data);

  datastream >> end;
  if (datastream.fail())
    data.clear();

  return true;
}

//convert . or , to the current locale for correct conversion of ascii float
void ConvertFloatLocale(std::string& strfloat)
{
  static struct lconv* locale = localeconv();
  
  size_t pos = strfloat.find_first_of(",.");

  while (pos != string::npos)
  {
    strfloat.replace(pos, 1, 1, *locale->decimal_point);
    pos++;

    if (pos >= strfloat.size())
      break;

    pos = strfloat.find_first_of(",.", pos);
  }
}

bool GetHomePath(std::string& homepath)
{
  const char* homeptr = getenv("HOME");
  if (homeptr && strlen(homeptr) > 0)
  {
    homepath = PutSlashAtEnd(homeptr);
    return true;
  }
  else
  {
    return false;
  }
}

std::string PutSlashAtEnd(const std::string& path)
{
  if (!path.empty() && path[path.length() - 1] != '/')
    return path + '/';
  else
    return path;
}

std::string RemoveSlashAtEnd(const std::string& path)
{
  if (path.length() > 0 && path[path.length() - 1] == '/')
    return path.substr(0, path.length() - 1);
  else
    return path;
}

std::string FileNameExtension(const std::string& path)
{
  size_t pos = path.rfind('.');
  if (pos == string::npos || pos >= path.length() - 1)
    return "";
  else
    return path.substr(pos + 1);
}

std::string ToLower(const std::string& in)
{
  string out;
  for (string::const_iterator it = in.begin(); it != in.end(); it++)
    out += (char)tolower(*it);

  return out;
}

