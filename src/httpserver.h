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

//Backwards compatiblity, MHD_YES and MHD_NO were changed from #define to enum values.
#if defined(MHD_YES) && defined(MHD_NO)
  #define MHD_Result int
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

    void SetBindAddr(std::string bindaddr) { m_bindaddr = bindaddr; }
    void SetPort(int port) { m_port = port; }
    void SetIpv6(bool ipv6) { m_ipv6 = ipv6; }
    void SetHtmlDirectory(const char* dir);

    bool Start();
    bool IsStarted() { return m_daemon != NULL; }
    void Stop();
    void SignalStop();

  private:
    struct MHD_Daemon* m_daemon;
    std::string        m_bindaddr;
    struct addrinfo*   m_bindaddrinfo;
    int                m_port;
    bool               m_ipv6;
    CBobDSP&           m_bobdsp;
    CMutex             m_mutex;
    int64_t            m_postdatasize;
    bool               m_stop;
    std::string        m_htmldir;
    bool               m_wasstarted;

    void               StartDaemonAny(unsigned int options, unsigned int timeout, unsigned int limittotal, unsigned int limitindividual);
    void               StartDaemonSpecific(unsigned int options, unsigned int timeout, unsigned int limittotal, unsigned int limitindividual);

    static MHD_Result  AnswerToConnection(void *cls, struct MHD_Connection *connection,
                                          const char *url, const char *method,
                                          const char *version, const char *upload_data,
                                          size_t *upload_data_size, void **con_cls);

    static void        RequestCompleted(void* cls, struct MHD_Connection* connection,
                                        void** con_cls, enum MHD_RequestTerminationCode toe);
    static void        SenderInfo(struct MHD_Connection* connection, std::string& host, std::string& port);

    static MHD_Result  CreateError(struct MHD_Connection *connection, int errorcode);
    static MHD_Result  CreateRedirect(struct MHD_Connection *connection, const std::string& location);
    static MHD_Result  CreateFileDownload(struct MHD_Connection *connection, const std::string& url,
                                          const std::string& root = "", const char* mime = NULL,
                                          bool checksize = true);
    static RETHTSIZE   FileReadCallback(void *cls, uint64_t pos, char *buf, ARGHTSIZE max);
    static void        FileReadFreeCallback(void* cls);

    static MHD_Result  CreateJSONDownload(struct MHD_Connection* connection, const std::string& json);
    static MHD_Result  CreateJSONDownload(struct MHD_Connection* connection, CJSONGenerator* generator);
    static RETHTSIZE   JSONReadCallback(void *cls, uint64_t pos, char *buf, ARGHTSIZE max);
    static void        JSONReadFreeCallback(void* cls);
};

#endif //HTTPSERVER_H
