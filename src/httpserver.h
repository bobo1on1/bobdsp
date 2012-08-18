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
#include <string>
#include <cstdarg>
#include <cstdio>
#include <sys/socket.h>
#include <microhttpd.h>

#if (MHD_VERSION >= 0x00090200)
#define HTSIZE ssize_t
#else
#define HTSIZE int
#endif

class CBobDSP;

class CHttpServer
{
  public:
    CHttpServer(CBobDSP& bobdsp);
    ~CHttpServer();

    bool Start();
    bool IsStarted() { return m_daemon != NULL; }
    void Stop();

  private:
    struct MHD_Daemon* m_daemon;
    int                m_port;
    CBobDSP&           m_bobdsp;

    static int AnswerToConnection (void *cls, struct MHD_Connection *connection,
                                   const char *url, const char *method,
                                   const char *version, const char *upload_data,
                                   size_t *upload_data_size, void **con_cls);

    static int    CreateErrorResponse(struct MHD_Connection *connection, int errorcode);
    static int    CreateFileDownloadResponse(struct MHD_Connection *connection, std::string filename, const char* mime = NULL);
    static HTSIZE FileReadCallback(void *cls, uint64_t pos, char *buf, HTSIZE max);
    static void   FileReadFreeCallback(void* cls);

    static int    CreateJSONDownloadResponse(struct MHD_Connection* connection, const std::string& json);
};

#endif //HTTPSERVER_H