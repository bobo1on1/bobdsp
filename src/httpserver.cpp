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

#include "httpserver.h"
#include "util/log.h"
#include "util/misc.h"
#include "bobdsp.h"

#include <fcntl.h>
#include <sys/stat.h>

using namespace std;

CHttpServer::CHttpServer(CBobDSP& bobdsp):
  m_bobdsp(bobdsp)
{
  m_daemon = NULL;
  m_port = 8080; //TODO: make configurable
}

CHttpServer::~CHttpServer()
{
  Stop();
}

bool CHttpServer::Start()
{
  if (m_daemon)
    return true;

  unsigned int options = MHD_USE_SELECT_INTERNALLY;
  if (g_printdebuglevel)
    options |= MHD_USE_DEBUG;

  m_daemon = MHD_start_daemon(options, m_port, NULL, NULL, 
                              &AnswerToConnection, this, MHD_OPTION_END);
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
  LogDebug("method: \"%s\" url: \"%s\"", method, url);

  CHttpServer* httpserver = (CHttpServer*)cls;

  string strurl = RemoveSlashAtEnd(url);

  if (strcmp(method, "GET") == 0)
  {
    if (strurl == "/log")
    {
      if (!g_logfilename.empty())
        return CreateFileDownloadResponse(connection, g_logfilename, "text/plain");
    }
    else if (strurl == "/connections")
    {
      return CreateJSONDownloadResponse(connection, httpserver->m_bobdsp.PortConnector().ConnectionsToJSON());
    }
    else
    {
      //TODO: add some security, since now you can download every file
      return CreateFileDownloadResponse(connection, string("html") + strurl);
    }
  }

  return CreateErrorResponse(connection, MHD_HTTP_NOT_FOUND);
}

int CHttpServer::CreateErrorResponse(struct MHD_Connection *connection, int errorcode)
{
  string errorstr = ToString(errorcode);

  struct MHD_Response* response = MHD_create_response_from_data(errorstr.length(), (void*)errorstr.c_str(), MHD_NO, MHD_YES);
  MHD_add_response_header(response, "Content-Type", "text/plain");

  int returnv = MHD_queue_response(connection, errorcode, response);
  MHD_destroy_response(response);

  return returnv;
}

int CHttpServer::CreateFileDownloadResponse(struct MHD_Connection *connection, std::string filename, const char* mime /*= NULL*/)
{
  int returnv;
  struct stat statinfo;
  returnv = stat(filename.c_str(), &statinfo);
  if (returnv == -1)
  {
    LogError("Unable to stat \"%s\": \"%s\"", filename.c_str(), GetErrno().c_str());
    return CreateErrorResponse(connection, MHD_HTTP_NOT_FOUND);
  }

  if (S_ISDIR(statinfo.st_mode))
    filename += "/index.html";

  LogDebug("Opening \"%s\"", filename.c_str());

  int fd = open(filename.c_str(), O_RDONLY);
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
    else
      mime = "text/plain";
  }

  int* hfd = new int(fd);
  struct MHD_Response* response = MHD_create_response_from_callback(-1, 10240, FileReadCallback, (void*)hfd, FileReadFreeCallback);
  MHD_add_response_header(response, "Content-Type", mime);
  returnv = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return returnv;
}

HTSIZE CHttpServer::FileReadCallback(void *cls, uint64_t pos, char *buf, HTSIZE max)
{
  int fd = *(int*)cls;

  ssize_t bytesread;
  do
  {
    bytesread = pread(fd, buf, max, pos);
    if (bytesread == -1)
      LogError("Reading file: \"%s\"", GetErrno().c_str());
  }
  while (bytesread == -1 && errno == EINTR);

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

