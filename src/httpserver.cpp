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

#include "config.h"
#include "bobdsp.h"
#include "httpserver.h"
#include "util/log.h"
#include "util/misc.h"
#include "util/lock.h"
#include "util/thread.h"

#ifndef _GNU_SOURCE
  #define _GNU_SOURCE //for canonicalize_file_name
#endif //_GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <uriparser/Uri.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <memory>

using namespace std;

#define POSTDATA_SIZELIMIT (100 * 1024 * 1024) //100 mb

CHttpServer::CHttpServer(CBobDSP& bobdsp):
  m_bobdsp(bobdsp)
{
  m_daemon = NULL;
  m_port = 8080;
  m_ipv6 = true;
  m_postdatasize = 0;
  m_stop = false;
  m_htmldir = RemoveSlashAtEnd(PREFIX) + "/share/bobdsp/html";
  m_wasstarted = true;
}

CHttpServer::~CHttpServer()
{
  Stop();
}

void CHttpServer::SetHtmlDirectory(const char* dir)
{
  char* path = canonicalize_file_name(dir);
  if (path == NULL)
  {
    LogError("canonicalize_file_name(): \"%s\": %s", dir, GetErrno().c_str());
    m_htmldir = RemoveSlashAtEnd(dir);
  }
  else
  {
    m_htmldir = path;
    free(path);
  }
}

bool CHttpServer::Start()
{
  if (m_daemon)
    return true;

  unsigned int options = MHD_USE_THREAD_PER_CONNECTION;

  if (m_ipv6)
    options |= MHD_USE_IPv6;

  if (g_printdebuglevel)
    options |= MHD_USE_DEBUG;

  //libmicrohttpd starts a thread of its own, it inherits the threadname it was started from
  //use a trick to set the thread name of libmicrohttpd's thread
  string threadname = CThread::GetCurrentThreadName();
  CThread::SetCurrentThreadName("httpserver");

  //two minutes timeout, has to be higher than one minute
  //since waiting for a change has a maximum timeout of one minute
  unsigned int timeout = 120;
  unsigned int limittotal = 500;
  unsigned int limitindividual = 100;
  m_daemon = MHD_start_daemon(options, m_port, NULL, NULL, 
                              &AnswerToConnection, this,
                              MHD_OPTION_CONNECTION_TIMEOUT, timeout,
                              MHD_OPTION_CONNECTION_LIMIT, limittotal,
                              MHD_OPTION_PER_IP_CONNECTION_LIMIT, limitindividual,
                              MHD_OPTION_NOTIFY_COMPLETED, RequestCompleted, NULL,
                              MHD_OPTION_END);

  CThread::SetCurrentThreadName(threadname);

  if (m_daemon)
    Log("Started webserver on port %i, using \"%s\" as html root directory", m_port, m_htmldir.c_str());
  else if (m_wasstarted || g_printdebuglevel)
    LogError("Unable to start webserver on port %i reason: \"%s\"", m_port, GetErrno().c_str());

  m_wasstarted = m_daemon != NULL;

  return m_daemon != NULL;
}

void CHttpServer::Stop()
{
  if (m_daemon)
  {
    Log("Stopping webserver");
    MHD_stop_daemon(m_daemon);
    m_daemon = NULL;
  }
}

void CHttpServer::SignalStop()
{
  m_stop = true;
}

int CHttpServer::AnswerToConnection(void *cls, struct MHD_Connection *connection,
                                    const char *url, const char *method,
                                    const char *version, const char *upload_data,
                                    size_t *upload_data_size, void **con_cls)
{
  CHttpServer* httpserver = (CHttpServer*)cls;

  //deny all requests when stopped
  if (httpserver->m_stop)
    return MHD_NO;

  //set thread name to current method and url
  CThread::SetCurrentThreadName(string(method) + " " + url);

  string host;
  string port;
  SenderInfo(connection, host, port);

  LogDebug("%s method: \"%s\" version: \"%s\" url: \"%s\"", host.c_str(), method, version, url);

  //convert percent encoded chars
  char* tmpurl = new char[strlen(url) + 1];
  strcpy(tmpurl, url);
  uriUnescapeInPlaceA(tmpurl);

  //make sure any url has a slash at the start
  string strurl = PutSlashAtStart(RemoveDuplicates(tmpurl, '/'));
  delete[] tmpurl;

  if (strcmp(method, "GET") == 0)
  {
    if (strurl == "/log")
    {
      if (!g_logfilename.empty() && g_logmutex)
      {
        CLock lock(*g_logmutex);
        //set checksize to false since there might be writes to the logfile after the size has been checked
        return CreateFileDownload(connection, g_logfilename, "", "text/plain", false);
      }
    }
    else if (strurl == "/connections")
    {
      return CreateJSONDownload(connection, httpserver->m_bobdsp.PortConnector().ConnectionsToJSON());
    }
    else if (strurl == "/ports")
    {
      return CreateJSONDownload(connection, httpserver->m_bobdsp.PortConnector().PortsToJSON());
    }
    else if (strurl == "/plugins")
    {
      return CreateJSONDownload(connection, httpserver->m_bobdsp.PluginManager().PluginsToJSON());
    }
    else if (strurl == "/clients")
    {
      return CreateJSONDownload(connection, httpserver->m_bobdsp.ClientsManager().ClientsToJSON(false));
    }
    else
    {
      return CreateFileDownload(connection, strurl, httpserver->m_htmldir.c_str());
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
      unique_ptr<CPostData> datadelete((CPostData*)*con_cls);

      if (((CPostData*)*con_cls)->error)
        return CreateError(connection, MHD_HTTP_INSUFFICIENT_STORAGE);

      std::string& postdata = ((CPostData*)*con_cls)->data;

      CLock lock(httpserver->m_mutex);
      httpserver->m_postdatasize -= postdata.length();
      lock.Leave();

      LogDebug("%s %s", host.c_str(), postdata.c_str());

      if (strurl == "/connections")
      {
        CJSONGenerator* generator = httpserver->m_bobdsp.PortConnector().LoadString(postdata, host, true);
        return CreateJSONDownload(connection, generator);
      }
      else if (strurl == "/ports")
      {
        return CreateJSONDownload(connection, httpserver->m_bobdsp.PortConnector().PortsToJSON(postdata, host));
      }
      else if (strurl == "/clients")
      {
        CJSONGenerator* generator = httpserver->m_bobdsp.ClientsManager().LoadString(postdata, host, true);
        return CreateJSONDownload(connection, generator);
      }
    }
  }

  return CreateError(connection, MHD_HTTP_NOT_FOUND);
}

void CHttpServer::RequestCompleted(void* cls, struct MHD_Connection* connection,
                                   void** con_cls, enum MHD_RequestTerminationCode toe)
{
  string host;
  string port;
  SenderInfo(connection, host, port);

  //when the request is completed, set the thread name to the http port,
  //in case of http keep-alive this thread will keep running until the connection is closed
  CThread::SetCurrentThreadName(string("http:") + port);
}

void CHttpServer::SenderInfo(struct MHD_Connection* connection, std::string& host, std::string& port)
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

      port = servbuf;
    }
    else
    {
      LogError("getnameinfo(): %s", gai_strerror(returnv));
      host = "unknown";
      port.clear();
    }
  }
  else
  {
    host = "unknown";
    port.clear();
  }
}

int CHttpServer::CreateError(struct MHD_Connection *connection, int errorcode)
{
  string errorstr = ToString(errorcode) + "\n";

  struct MHD_Response* response = MHD_create_response_from_buffer(errorstr.length(), (void*)errorstr.c_str(), MHD_RESPMEM_MUST_COPY);
  MHD_add_response_header(response, "Content-Type", "text/plain");
  MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");

  int returnv = MHD_queue_response(connection, errorcode, response);
  MHD_destroy_response(response);

  return returnv;
}

int CHttpServer::CreateRedirect(struct MHD_Connection *connection, const std::string& location)
{
  LogDebug("Creating redirect to %s", location.c_str());

  string html = "<html> <head> <title>Moved</title> </head> <body> <h1>Moved</h1> <p>This page has moved to <a href=\"";
  html += location;
  html += "\">";
  html += location;
  html += "</a>.</p> </body> </html>";

  struct MHD_Response* response = MHD_create_response_from_buffer(html.length(), (void*)html.c_str(), MHD_RESPMEM_MUST_COPY);
  MHD_add_response_header(response, "Content-Type", "text/html");
  MHD_add_response_header(response, "Location", location.c_str());
  MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");

  int returnv = MHD_queue_response(connection, MHD_HTTP_MOVED_PERMANENTLY, response);
  MHD_destroy_response(response);

  return returnv;
}

int CHttpServer::CreateFileDownload(struct MHD_Connection *connection, const std::string& url,
                                    const std::string& root /*= ""*/, const char* mime /*= NULL*/,
                                    bool checksize /*= true*/)
{
  string filename = root + url;

  //make sure no file outside the root is accessed
  if (!root.empty() && DirLevel(url) < 0)
  {
    LogError("Not allowing access to \"%s\", it's outside the root", filename.c_str());
    return CreateError(connection, MHD_HTTP_FORBIDDEN);
  }

  int returnv;
  struct stat64 statinfo;
  returnv = stat64(filename.c_str(), &statinfo);
  if (returnv == -1)
  {
    LogError("Unable to stat \"%s\": \"%s\"", filename.c_str(), GetErrno().c_str());
    return CreateError(connection, MHD_HTTP_NOT_FOUND);
  }
  else if (S_ISDIR(statinfo.st_mode))
  {
    //if the url is a directory, and has a slash at the end, return index.html
    //if it doesn't have a slash at the end, return a redirect to the url with a slash added
    if (url.size() >= 1 && url[url.size() - 1] == '/')
      return CreateFileDownload(connection, url + "index.html", root, mime);
    else
      return CreateRedirect(connection, PutSlashAtEnd(url));
  }

  if (checksize)
    LogDebug("Opening \"%s\" size:%" PRIi64, filename.c_str(), (int64_t)statinfo.st_size);
  else
    LogDebug("Opening \"%s\"", filename.c_str());

  int fd = open64(filename.c_str(), O_RDONLY);
  if (fd == -1)
  {
    LogError("Unable to open \"%s\": \"%s\"", filename.c_str(), GetErrno().c_str());
    return CreateError(connection, MHD_HTTP_NOT_FOUND);
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

  int64_t size = checksize ? statinfo.st_size : -1;
  int64_t block = checksize ? Clamp(statinfo.st_size, 1, 10 * 1024 * 1024) : 10240;
  int* hfd = new int(fd);
  struct MHD_Response* response = MHD_create_response_from_callback(size, block, FileReadCallback,
                                                                    (void*)hfd, FileReadFreeCallback);
  MHD_add_response_header(response, "Content-Type", mime);
  MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
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

int CHttpServer::CreateJSONDownload(struct MHD_Connection* connection, const std::string& json)
{
  struct MHD_Response* response = MHD_create_response_from_buffer(json.length(), (void*)json.c_str(), MHD_RESPMEM_MUST_COPY);
  MHD_add_response_header(response, "Content-Type", "application/json");
  MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
  int returnv = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return returnv;
}

//accepts a CJSONGenerator which is allocated in one of the managers
//this way the callback can read directly from the libyajl buffer
//and delete the CJSONGenerator afterwards
int CHttpServer::CreateJSONDownload(struct MHD_Connection* connection, CJSONGenerator* generator)
{
  if (generator)
  {
    uint64_t size = generator->GetGenBufSize();
    struct MHD_Response* response = MHD_create_response_from_callback(size, Clamp(size, (uint64_t)1, (uint64_t)10 * 1024 * 1024),
                                                                      JSONReadCallback, generator, JSONReadFreeCallback);
    MHD_add_response_header(response, "Content-Type", "application/json");
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    int returnv = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return returnv;
  }
  else
  {
    return MHD_NO;
  }
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

