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

#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include "util/inclstdint.h"
#include "util/mutex.h"
#include "util/JSON.h"
#include <string>
#include <cstdarg>
#include <cstdio>
#include <sys/socket.h>
#include <microhttpd.h>

#if (MHD_VERSION >= 0x00090200)
  #define RETHTSIZE ssize_t
  #if (MHD_VERSION >= 0x00091400) //TODO: check if this version check is ok
    #define ARGHTSIZE size_t
  #else
    #define ARGHTSIZE ssize_t
  #endif
#else
  #define RETHTSIZE int
  #define ARGHTSIZE int
#endif

class CBobDSP;

class CPostData
{
  public:
    CPostData()
    {
      error = false;
    }

    std::string data;
    bool error;
};

class CHttpServer
{
  public:
    CHttpServer(CBobDSP& bobdsp);
    ~CHttpServer();

    void SetPort(int port) { m_port = port; }

    bool Start();
    bool IsStarted() { return m_daemon != NULL; }
    void Stop();

  private:
    struct MHD_Daemon* m_daemon;
    int                m_port;
    CBobDSP&           m_bobdsp;
    CMutex             m_mutex;
    int64_t            m_postdatasize;

    static int       AnswerToConnection (void *cls, struct MHD_Connection *connection,
                                         const char *url, const char *method,
                                         const char *version, const char *upload_data,
                                         size_t *upload_data_size, void **con_cls);

    static int       CreateError(struct MHD_Connection *connection, int errorcode);
    static int       CreateFileDownload(struct MHD_Connection *connection, std::string filename,
                                                const std::string& root = "", const char* mime = NULL);
    static RETHTSIZE FileReadCallback(void *cls, uint64_t pos, char *buf, ARGHTSIZE max);
    static void      FileReadFreeCallback(void* cls);

    static int       CreateJSONDownload(struct MHD_Connection* connection, const std::string& json);
    static int       CreateJSONDownload(struct MHD_Connection* connection, CJSONGenerator* generator);
    static RETHTSIZE JSONReadCallback(void *cls, uint64_t pos, char *buf, ARGHTSIZE max);
    static void      JSONReadFreeCallback(void* cls);
};

#endif //HTTPSERVER_H
