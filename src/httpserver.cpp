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

#include "bobdsp.h"
#include "httpserver.h"
#include "util/log.h"
#include "util/misc.h"
#include "util/lock.h"

#ifndef _GNU_SOURCE
  #define _GNU_SOURCE //for pipe2
#endif //_GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <uriparser/Uri.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <memory>

using namespace std;

#define POSTDATA_SIZELIMIT (100 * 1024 * 1024) //100 mb

CHttpServer::CHttpServer(CBobDSP& bobdsp):
  m_bobdsp(bobdsp)
{
  m_daemon = NULL;
  m_port = 8080;
  m_postdatasize = 0;
}

CHttpServer::~CHttpServer()
{
  Stop();
}

bool CHttpServer::Start()
{
  if (m_daemon)
    return true;

  unsigned int options = MHD_USE_THREAD_PER_CONNECTION | MHD_USE_IPv6;
  if (g_printdebuglevel)
    options |= MHD_USE_DEBUG;

  unsigned int timeout = 60 * 60; //one hour timeout
  m_daemon = MHD_start_daemon(options, m_port, NULL, NULL, 
                              &AnswerToConnection, this, MHD_OPTION_CONNECTION_TIMEOUT, timeout, MHD_OPTION_END);
  if (m_daemon)
    Log("Started webserver on port %i", m_port);
  else
    LogError("Unable to start webserver on port %i reason: \"%s\"", m_port, GetErrno().c_str());

  return m_daemon != NULL;
}

void CHttpServer::Stop()
{
  if (m_daemon)
  {
    MHD_stop_daemon(m_daemon);
    m_daemon = NULL;
  }
}

int CHttpServer::AnswerToConnection(void *cls, struct MHD_Connection *connection,
                                    const char *url, const char *method,
                                    const char *version, const char *upload_data,
                                    size_t *upload_data_size, void **con_cls)
{
  const MHD_ConnectionInfo* connectioninfo = MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);

  sockaddr* sock = (sockaddr*)connectioninfo->client_addr;
  socklen_t size;
  if (sock->sa_family == AF_INET)
    size = sizeof(sockaddr_in);
  else if (sock->sa_family == AF_INET6)
    size = sizeof(sockaddr_in6);
  else
  {
    size = 0;
    LogError("Unknown protocol %u", sock->sa_family);
  }

  //convert ip:port to string
  char hostbuf[NI_MAXHOST];
  char servbuf[NI_MAXSERV];
  string host;
  if (size > 0)
  {
    //pass NI_NUMERICHOST to prevent a dns lookup, this might take a long time
    //and will slow down the http request
    int returnv = getnameinfo(sock, size, hostbuf, sizeof(hostbuf), servbuf,
                              sizeof(servbuf), NI_NUMERICHOST | NI_NUMERICSERV);
    if (returnv == 0)
    {
      if (sock->sa_family == AF_INET6)
        host = string("[") + hostbuf + "]:" + servbuf;
      else
        host = string(hostbuf) + ":" + servbuf;
    }
    else
    {
      LogError("getnameinfo(): %s", gai_strerror(returnv));
    }
  }

  if (host.empty())
    host = "unknown";

  CThread::SetCurrentThreadName((string("http ") + host).c_str());

  LogDebug("%s method: \"%s\" url: \"%s\"", host.c_str(), method, url);

  CHttpServer* httpserver = (CHttpServer*)cls;

  //convert percent encoded chars
  char* tmpurl = new char[strlen(url) + 1];
  strcpy(tmpurl, url);
  uriUnescapeInPlaceA(tmpurl);

  //make sure any url has no slash and end, and has a slash at the start
  string strurl = PutSlashAtStart(RemoveSlashAtEnd(tmpurl));
  delete[] tmpurl;

  if (strcmp(method, "GET") == 0)
  {
    if (strurl == "/log")
    {
      if (!g_logfilename.empty())
        return CreateFileDownloadResponse(connection, g_logfilename, "", "text/plain");
    }
    else if (strurl == "/connections")
    {
      return CreateJSONDownloadResponse(connection, httpserver->m_bobdsp.PortConnector().ConnectionsToJSON());
    }
    else if (strurl == "/ports")
    {
      return CreateJSONDownloadResponse(connection, httpserver->m_bobdsp.PortConnector().PortsToJSON());
    }
    else if (strurl == "/plugins")
    {
      return CreateJSONDownloadResponse(connection, httpserver->m_bobdsp.PluginManager().PluginsToJSON());
    }
    else if (strurl == "/clients")
    {
      return CreateJSONDownloadResponse(connection, httpserver->m_bobdsp.ClientsManager().ClientsToJSON(true));
    }
    else if (strurl == "/visualizer")
    {
      return CreateJSONDownloadResponse(connection, httpserver->m_bobdsp.Visualizer().JSON());
    }
    else
    {
      return CreateFileDownloadResponse(connection, strurl, "html");
    }
  }
  else if (strcmp(method, "POST") == 0)
  {
    LogDebug("%s upload_data_size: %zi", host.c_str(), *upload_data_size);

    if (!*con_cls) //on the first call, just alloc a string
    {
      *con_cls = new CPostData();
      return MHD_YES;
    }
    else if (*upload_data_size) //on every next call with data, process
    {
      if (!((CPostData*)*con_cls)->error)
      {
        std::string& postdata = ((CPostData*)*con_cls)->data;

        //use strnlen here to exclude any null characters
        size_t length = strnlen(upload_data, *upload_data_size);
        postdata.append(upload_data, length);

        //check if all post data combined doesn't use more than the memory limit
        CLock lock(httpserver->m_mutex);
        httpserver->m_postdatasize += length;
        if (httpserver->m_postdatasize > POSTDATA_SIZELIMIT)
        {
          LogError("%s hit post data size limit, %" PRIi64 " bytes allocated", host.c_str(), httpserver->m_postdatasize);
          httpserver->m_postdatasize -= postdata.length();
          ((CPostData*)*con_cls)->error = true;
          postdata.clear();
        }
      }

      *upload_data_size = 0; //signal that we processed this data

      return MHD_YES;
    }
    else 
    {
      auto_ptr<CPostData> datadelete((CPostData*)*con_cls);

      if (((CPostData*)*con_cls)->error)
        return CreateErrorResponse(connection, MHD_HTTP_INSUFFICIENT_STORAGE);

      std::string& postdata = ((CPostData*)*con_cls)->data;

      CLock lock(httpserver->m_mutex);
      httpserver->m_postdatasize -= postdata.length();
      lock.Leave();

      LogDebug("%s %s", host.c_str(), postdata.c_str());

      if (strurl == "/connections")
      {
        CJSONGenerator* generator = httpserver->m_bobdsp.PortConnector().LoadSettingsFromString(postdata, host, true);
        return CreateJSONDownloadResponse(connection, generator);
      }
      else if (strurl == "/ports")
      {
        return CreateJSONDownloadResponse(connection, httpserver->m_bobdsp.PortConnector().PortsToJSON(postdata, host));
      }
      else if (strurl == "/clients")
      {
        CJSONGenerator* generator = httpserver->m_bobdsp.ClientsManager().LoadSettingsFromString(postdata, host, true);
        return CreateJSONDownloadResponse(connection, generator);
      }
      else if (strurl == "/visualizer")
      {
        return CreateJSONDownloadResponse(connection, httpserver->m_bobdsp.Visualizer().JSON(postdata));
      }
    }
  }

  return CreateErrorResponse(connection, MHD_HTTP_NOT_FOUND);
}

int CHttpServer::CreateErrorResponse(struct MHD_Connection *connection, int errorcode)
{
  string errorstr = ToString(errorcode) + "\n";

  struct MHD_Response* response = MHD_create_response_from_data(errorstr.length(), (void*)errorstr.c_str(), MHD_NO, MHD_YES);
  MHD_add_response_header(response, "Content-Type", "text/plain");

  int returnv = MHD_queue_response(connection, errorcode, response);
  MHD_destroy_response(response);

  return returnv;
}

int CHttpServer::CreateFileDownloadResponse(struct MHD_Connection *connection, std::string filename,
                                            const std::string& root /*= ""*/, const char* mime /*= NULL*/)
{
  //make sure no file outside the root is accessed
  if (!root.empty() && DirLevel(filename) < 0)
  {
    LogError("Not allowing access to \"%s\", it's outside the root", string(root + filename).c_str());
    return CreateErrorResponse(connection, MHD_HTTP_FORBIDDEN);
  }

  filename = root + filename;

  int returnv;
  struct stat64 statinfo;
  returnv = stat64(filename.c_str(), &statinfo);
  if (returnv == -1)
  {
    LogError("Unable to stat \"%s\": \"%s\"", filename.c_str(), GetErrno().c_str());
    return CreateErrorResponse(connection, MHD_HTTP_NOT_FOUND);
  }

  if (S_ISDIR(statinfo.st_mode))
  {
    filename = PutSlashAtEnd(filename) + "index.html";
    returnv = stat64(filename.c_str(), &statinfo);
    if (returnv == -1)
    {
      LogError("Unable to stat \"%s\": \"%s\"", filename.c_str(), GetErrno().c_str());
      return CreateErrorResponse(connection, MHD_HTTP_NOT_FOUND);
    }
  }

  LogDebug("Opening \"%s\" size:%" PRIi64, filename.c_str(), (int64_t)statinfo.st_size);

  int fd = open64(filename.c_str(), O_RDONLY);
  if (fd == -1)
  {
    LogError("Unable to open \"%s\": \"%s\"", filename.c_str(), GetErrno().c_str());
    return CreateErrorResponse(connection, MHD_HTTP_NOT_FOUND);
  }

  if (!mime)
  {
    string extension = ToLower(FileNameExtension(filename));
    if (extension == "html")
      mime = "text/html";
    else if (extension == "js")
      mime = "application/javascript";
    else if (extension == "css")
      mime = "text/css";
    else if (extension == "png")
      mime = "image.png";
    else
      mime = "text/plain";
  }

  int* hfd = new int(fd);
  struct MHD_Response* response = MHD_create_response_from_callback(statinfo.st_size, Clamp(statinfo.st_size, 1, 10 * 1024 * 1024),
                                                                    FileReadCallback, (void*)hfd, FileReadFreeCallback);
  MHD_add_response_header(response, "Content-Type", mime);
  returnv = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return returnv;
}

RETHTSIZE CHttpServer::FileReadCallback(void *cls, uint64_t pos, char *buf, ARGHTSIZE max)
{
  int fd = *(int*)cls;

  ssize_t bytesread;
  int tmperrno;
  do
  {
    bytesread = pread64(fd, buf, max, pos);
    tmperrno = errno;
    if (bytesread == -1)
      LogError("Reading file: \"%s\"", GetErrno().c_str());
  }
  while (bytesread == -1 && tmperrno == EINTR);

  if (bytesread == 0 || bytesread == -1)
    return -1; //done reading, returning 0 here is against api
  else
    return bytesread;
}

void CHttpServer::FileReadFreeCallback(void* cls)
{
  int* fd = (int*)cls;
  if (*fd != -1)
    close(*fd);
  delete fd;
}

int CHttpServer::CreateJSONDownloadResponse(struct MHD_Connection* connection, const std::string& json)
{
  struct MHD_Response* response = MHD_create_response_from_data(json.length(), (void*)json.c_str(), MHD_NO, MHD_YES);
  MHD_add_response_header(response, "Content-Type", "application/json");
  int returnv = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return returnv;
}

//accepts a CJSONGenerator which is allocated in one of the managers
//this way the callback can read directly from the libyajl buffer
//and delete the CJSONGenerator afterwards
int CHttpServer::CreateJSONDownloadResponse(struct MHD_Connection* connection, CJSONGenerator* generator)
{
  uint64_t size = generator->GetGenBufSize();
  struct MHD_Response* response = MHD_create_response_from_callback(size, Clamp(size, (uint64_t)1, (uint64_t)10 * 1024 * 1024),
                                                                    JSONReadCallback, generator, JSONReadFreeCallback);
  MHD_add_response_header(response, "Content-Type", "application/json");
  int returnv = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return returnv;
}

RETHTSIZE CHttpServer::JSONReadCallback(void *cls, uint64_t pos, char *buf, ARGHTSIZE max)
{
  CJSONGenerator* generator = (CJSONGenerator*)cls;
  uint64_t size = Min(generator->GetGenBufSize() - pos, (uint64_t)max);
  if (size == 0)
    return -1; //done reading

  memcpy(buf, generator->GetGenBuf() + pos, size);
  return size;
}

void CHttpServer::JSONReadFreeCallback(void* cls)
{
  delete (CJSONGenerator*)cls;
}

