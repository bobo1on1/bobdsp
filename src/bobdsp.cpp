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

#ifndef _GNU_SOURCE
  #define _GNU_SOURCE //for pipe2
#endif //_GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>

#include "bobdsp.h"

#include <cstring>
#include <cstdlib>
#include <malloc.h>
#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/signalfd.h>
#include <locale.h>
#include <getopt.h>

#include "util/log.h"
#include "util/misc.h"
#include "util/timeutils.h"
#include "util/JSON.h"

#define PORTRETRY         15625
#define TIMEOUT_INFINITE  -1000

using namespace std;

CBobDSP::CBobDSP(int argc, char *argv[]):
  m_portconnector(*this),
  m_clientsmanager(*this),
  m_httpserver(*this)
{
  m_stop            = false;
  m_checkconnect    = false;
  m_checkdisconnect = false;
  m_updateports     = false;
  m_signalfd        = -1;

  m_stdout[0] = m_stdout[1] = -1;
  m_stderr[0] = m_stderr[1] = -1;

  //make sure all numeric string<->float conversions are done using the C locale
  setlocale(LC_NUMERIC, "C");


  struct option longoptions[] =
  {
   {"debug",    no_argument,       NULL, 'd'},
   {"fork",     no_argument,       NULL, 'f'},
   {"help",     no_argument,       NULL, 'h'},
   {"port",     required_argument, NULL, 'p'},
   {"html-dir", required_argument, NULL, 't'},
   {0, 0, 0, 0}
  };

  const char* shortoptions = "dfhp:t:";
  int         optionindex = 0;
  int         c;
  bool        dofork = false;

  while ((c = getopt_long(argc, argv, shortoptions, longoptions, &optionindex)) != -1)
  {
    if (c == 'd')
    {
      g_printdebuglevel = true;
    }
    else if (c == 'f')
    {
      dofork = true;
    }
    else if (c == 'p')
    {
      int port;
      if (!StrToInt(optarg, port) || port < 0 || port > 65535)
      {
        fprintf(stderr, "Error: invalid argument %s for port\n", optarg);
        exit(1);
      }

      m_httpserver.SetPort(port);
    }
    else if (c == 't')
    {
      m_httpserver.SetHtmlDirectory(optarg);
    }
    else if (c == 'h' || c == '?')
    {
      PrintHelpMessage();
      exit(1);
    }
  }

  if (dofork)
  {
    g_logtostderr = false;

    if (g_printdebuglevel)
    {
      //route stdout to our log
      RoutePipe(stdout, m_stdout);
      //route stderr to our log
      RoutePipe(stderr, m_stderr);
    }
    else
    {
      fclose(stdout);
      stdout = fopen("/dev/null", "w");
      fclose(stderr);
      stderr = fopen("/dev/null", "w");
    }

    if (fork())
      exit(0);
  }
}

CBobDSP::~CBobDSP()
{
}

void CBobDSP::PrintHelpMessage()
{
  printf(
         "\n"
         "usage: bobdsp [OPTION]\n"
         "\n"
         "  options:\n"
         "\n"
         "    -d, --debug       enable debug logging\n"
         "    -f, --fork        daemonize, suppresses logging to stderr\n"
         "    -p, --port [PORT] set the port for the http server\n"
         "    -h, --help        print this message\n"
         "    -t, --html-dir    set the html directory\n"
         "\n"
         );
}

void CBobDSP::Setup()
{
  //set up locked memory for better realtime performance
  //SetupRT(1024 * 1024 * 10); //10 mb

  //init the logfile
  SetLogFile("bobdsp.log");

  //get paths to load ladspa plugins from
  vector<string> ladspapaths;
  LoadLadspaPaths(ladspapaths);

  //load ladspa plugins
  m_pluginmanager.LoadPlugins(ladspapaths);

  //load clients, connections and visualizers settings
  LoadSettings();

  //set up signal handlers
  SetupSignals();

  //set up jack logging
  jack_set_error_function(JackError);
  jack_set_info_function(JackInfo);
}

void CBobDSP::Process()
{
  int64_t timeout = TIMEOUT_INFINITE;
  int64_t portretryinterval = 0;
  //set up timestamp so we connect on the first iteration
  int64_t lastconnect = GetTimeUs() - CONNECTINTERVAL;
  //make sure to retrieve the ports list
  m_updateports = true;

  m_visualizer.Start();

  while(!m_stop)
  {
    bool triedconnect = false;
    bool allconnected = true;

    //check if we need to start the http server
    if (!m_httpserver.IsStarted())
    {
      allconnected = false; //use timeout in ProcessMessages()
      if (GetTimeUs() - lastconnect >= CONNECTINTERVAL)
      {
        m_httpserver.Start();
        triedconnect = true; //update timestamp
      }
    }

    if (m_clientsmanager.Process(triedconnect, allconnected, lastconnect))
    {
      m_checkconnect = true;
      m_updateports = true;
    }

    if (triedconnect)
      lastconnect = GetTimeUs();

    //if a client connected, or a port callback was called
    //process port connections, if it fails try again next time
    m_portconnector.Process(m_checkconnect, m_checkdisconnect, m_updateports);

    if (m_checkconnect || m_checkdisconnect || m_updateports)
    {
      //if the portconnector failed to process, retry after PORTRETRY,
      //and multiply the retry time by 2 after each try, up to CONNECTINTERVAL
      //the reason for this is that bobdsp might try to connect client ports
      //before the client is made active
      if (portretryinterval == 0)
      {
        portretryinterval = PORTRETRY;
      }
      else
      {
        portretryinterval *= 2;
        if (portretryinterval > CONNECTINTERVAL)
          portretryinterval = CONNECTINTERVAL;
      }
      timeout = portretryinterval;
    }
    else if (!allconnected)
    {
      //if not all clients are connected, retry after CONNECTINTERVAL
      timeout = CONNECTINTERVAL;
      //reset port connector interval
      portretryinterval = 0;
    }
    else
    {
      //everything ok, wait for events
      timeout = TIMEOUT_INFINITE;
      portretryinterval = 0;
    }

    //process messages, blocks if there's nothing to do
    ProcessMessages(timeout);

    LogDebug("main loop woke up");
  }
}

void CBobDSP::Cleanup()
{
  m_httpserver.SignalStop();
  m_visualizer.AsyncStopThread();
  m_portconnector.Stop();
  m_clientsmanager.Stop();
  m_visualizer.StopThread();
  m_httpserver.Stop();
  m_pluginmanager.UnloadPlugins();

  if (m_signalfd != -1)
    close(m_signalfd);
}

//based on https://rt.wiki.kernel.org/index.php/Dynamic_memory_allocation_example
void CBobDSP::SetupRT(int64_t memsize)
{
  Log("Setting up %" PRIi64 " MB locked memory", Round64((double)memsize / 1024.0 / 1024.0));

  //lock ALL the ram _o/
  if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
    LogError("calling mlockall: %s", GetErrno().c_str());

  //set malloc trimming to twice memsize
  //this way, if the amount of locked unallocated ram goes above the threshold, it's released
  if (mallopt(M_TRIM_THRESHOLD, memsize * 2) != 1)
    LogError("calling mallopt with M_TRIM_THRESHOLD %" PRIi64 " %s", memsize * 2, GetErrno().c_str());

  //set malloc top padding to memsize
  //this makes sure that at least memsize bytes stay locked
  if (mallopt(M_TOP_PAD, memsize) != 1)
    LogError("calling mallopt with M_TOP_PAD %" PRIi64 " %s", memsize, GetErrno().c_str());

  //Turn off mmap usage.
  if (mallopt(M_MMAP_MAX, 0) != 1)
    LogError("calling mallopt with M_MMAP_MAX 0: %s", GetErrno().c_str());

  long pagesize = sysconf(_SC_PAGESIZE);
  if (pagesize == -1)
    LogError("calling sysconf with _SC_PAGESIZE: %s", GetErrno().c_str());

  if (pagesize <= 0)
  {
    if (pagesize != -1)
      LogError("got invalid pagesize %li from sysconf", pagesize);

    pagesize = 1; //safe fallback
  }

  //Touch each page in this piece of memory to get it mapped into RAM
  //Each write to this buffer will generate a pagefault.
  //Once the pagefault is handled a page will be locked in memory and never
  //given back to the system.
  uint8_t* buffer = (uint8_t*)malloc(memsize);
  for (int64_t i = 0; i < memsize; i += pagesize)
    buffer[i] = 0;

  free(buffer);
  //buffer is now released. As glibc is configured such that it never gives back memory to
  //the kernel, the memory allocated above is locked for this process. All malloc() and new()
  //calls come from the memory pool reserved and locked above. Issuing free() and delete()
  //does NOT make this locking undone. So, with this locking mechanism we can build C++ applications
  //that will never run into a major/minor pagefault, even with swapping enabled.
}

void CBobDSP::SetupSignals()
{
  m_signalfd = -1;

  sigset_t sigset;
  if (sigemptyset(&sigset) == -1)
  {
    LogError("sigemptyset: %s", GetErrno().c_str());
    return;
  }

  if (sigaddset(&sigset, SIGTERM) == -1)
  {
    LogError("adding SIGTERM: %s", GetErrno().c_str());
    return;
  }

  if (sigaddset(&sigset, SIGINT) == -1)
  {
    LogError("adding SIGINT: %s", GetErrno().c_str());
    return;
  }

  //create a file descriptor that will catch SIGTERM and SIGINT
  m_signalfd = signalfd(-1, &sigset, SFD_NONBLOCK);
  if (m_signalfd == -1)
  {
    LogError("signalfd: %s", GetErrno().c_str());
  }
  else
  {
    //block SIGTERM and SIGINT
    if (sigprocmask(SIG_BLOCK, &sigset, NULL) == -1)
      LogError("sigpocmask: %s", GetErrno().c_str());
  }

  if (sigemptyset(&sigset) == -1)
  {
    LogError("sigemptyset: %s", GetErrno().c_str());
    return;
  }

  if (sigaddset(&sigset, SIGPIPE) == -1)
  {
    LogError("adding SIGPIPE: %s", GetErrno().c_str());
    return;
  }

  //libjack throws SIGPIPE a lot, block it
  if (sigprocmask(SIG_BLOCK, &sigset, NULL) == -1)
    LogError("sigpocmask: %s", GetErrno().c_str());
}

void CBobDSP::RoutePipe(FILE*& file, int* pipefds)
{
  int returnv = pipe2(pipefds, O_NONBLOCK);
  if (returnv == -1)
  {
    LogError("making pipe: %s", GetErrno().c_str());
    pipefds[0] = pipefds[1] = -1;
    return;
  }

  //close the FILE* first, then call dup
  //this will make sure that the dup will return 1 for stdout and 2 for stderr
  fclose(file);
  int fd = dup(pipefds[1]);
  if (fd == -1)
  {
    LogError("dup: %s", GetErrno().c_str());
    return;
  }

  file = fdopen(fd, "w");
  if (!file)
    LogError("fdopen: %s", GetErrno().c_str());
}

void CBobDSP::ProcessMessages(int64_t timeout)
{
  const int extra = 6;

  pollfd* fds;
  int nrclientpipes = m_clientsmanager.ClientPipes(fds, extra);
  unsigned int nrfds = nrclientpipes;

  int pipes[extra] = { m_stdout[0], m_stderr[0], m_signalfd, m_portconnector.MsgPipe(),
                       m_clientsmanager.MsgPipe(), m_visualizer.MsgPipe()};
  int pipenrs[extra] = { -1, -1, -1, -1, -1, -1 };

  for (size_t i = 0; i < sizeof(pipes) / sizeof(pipes[0]); i++)
  {
    if (pipes[i] != -1)
    {
      fds[nrfds].fd = pipes[i];
      fds[nrfds].events = POLLIN;
      pipenrs[i] = nrfds;
      nrfds++;
    }
  }

  timeout /= 1000;

  if (nrfds == 0)
  {
    LogDebug("no file descriptors to wait on");
    delete[] fds;
    sleep(1);
    return;
  }
  else
  {
    LogDebug("Waiting on %i file descriptors, timeout %s", nrfds, timeout >= 0 ? ToString(timeout).c_str() : "infinite");
  }

  int returnv = poll(fds, nrfds, timeout);
  if (returnv == -1)
  {
    LogError("poll on msg pipes: %s", GetErrno().c_str());
    sleep(1); //to prevent busy spinning in case we don't handle the error correctly
  }
  else if (returnv > 0)
  {
    //check client messages
    ProcessClientMessages(fds, nrclientpipes);
    
    //check stdout pipe
    if (pipenrs[0] != -1 && (fds[pipenrs[0]].revents & POLLIN))
      ProcessStdFd("stdout", m_stdout[0]);

    //check stderr pipe
    if (pipenrs[1] != -1 && (fds[pipenrs[1]].revents & POLLIN))
      ProcessStdFd("stderr", m_stderr[0]);

    //check for signals
    if (pipenrs[2] != -1 && (fds[pipenrs[2]].revents & POLLIN))
      ProcessSignalfd();

    //check for message from the http server
    if (pipenrs[3] != -1 && (fds[pipenrs[3]].revents & POLLIN))
      ProcessManagerMessages(m_portconnector);

    //check for message from the clients manager
    if (pipenrs[4] != -1 && (fds[pipenrs[4]].revents & POLLIN))
      ProcessManagerMessages(m_clientsmanager);

    //check for message from the clients manager
    if (pipenrs[5] != -1 && (fds[pipenrs[5]].revents & POLLIN))
      ProcessManagerMessages(m_visualizer);
  }

  delete[] fds;
}

void CBobDSP::ProcessClientMessages(pollfd* fds, int nrclientpipes)
{
  //if one of the jack clients has sent us a message, process messages of all jack clients
  for (int i = 0; i < nrclientpipes; i++)
  {
    if (fds[i].revents & POLLIN)
    {
      m_clientsmanager.ProcessMessages(m_checkconnect, m_checkdisconnect, m_updateports);
      break;
    }
  }
}

void CBobDSP::ProcessSignalfd()
{
  signalfd_siginfo siginfo;
  int returnv = read(m_signalfd, &siginfo, sizeof(siginfo));
  if (returnv == -1 && errno != EAGAIN)
  {
    int tmperrno = errno;
    LogError("reading signals fd: %s", GetErrno().c_str());
    if (tmperrno != EINTR)
    {
      close(m_signalfd);
      m_signalfd = -1;
    }
  }
  else if (returnv > 0)
  {
    if (siginfo.ssi_signo == SIGTERM || siginfo.ssi_signo == SIGINT)
    {
      Log("caught %s, exiting", siginfo.ssi_signo == SIGTERM ? "SIGTERM" : "SIGINT");
      m_stop = true;
    }
    else
    {
      LogDebug("caught signal %i", siginfo.ssi_signo);
    }
  }
}

void CBobDSP::ProcessStdFd(const char* name, int& fd)
{
  int returnv;
  char buf[1024];
  string logstr;
  while ((returnv = read(fd, buf, sizeof(buf) - 1)) > 0)
  {
    buf[returnv] = 0;
    logstr += buf;
  }
  int tmperrno = errno;

  if (!logstr.empty())
    LogDebug("%s: %s", name, logstr.c_str());

  if (returnv == -1 && tmperrno != EAGAIN)
  {
    LogError("reading %s fd: \"%s\"", name, GetErrno().c_str());
    if (tmperrno != EINTR)
    {
      close(fd);
      fd = -1;
    }
  }
}

void CBobDSP::ProcessManagerMessages(CMessagePump& manager)
{
  ClientMessage msg;
  while ((msg = manager.GetMessage()) != MsgNone)
  {
    LogDebug("got message %s from %s", MsgToString(msg), manager.Sender());
    if (msg == MsgConnectionsUpdated)
      m_checkconnect = m_checkdisconnect = true;
    else if (msg == MsgPortRegistered)
      m_updateports = m_checkconnect = true;
    else if (msg == MsgPortDeregistered)
      m_updateports = true;
    else if (msg == MsgPortConnected)
      m_checkdisconnect = true;
    else if (msg == MsgPortDisconnected)
      m_checkconnect = true;

    manager.ConfirmMessage(msg);
  }
}

void CBobDSP::LoadLadspaPaths(std::vector<std::string>& ladspapaths)
{
  //try to get paths from the LADSPA_PATH environment variable
  //paths are separated by colons
  const char* ladspaptr = getenv("LADSPA_PATH");
  if (ladspaptr)
    LogDebug("LADSPA_PATH = \"%s\"", ladspaptr);
  else
    LogDebug("LADSPA_PATH not set");

  if (ladspaptr)
  {
    string path = ladspaptr;
    size_t pos  = 0;
    for(;;)
    {
      size_t delim = path.find(':', pos);

      if (delim != string::npos)
      {
        if (delim > pos)
        {
          string foundpath = path.substr(pos, delim - pos);
          LogDebug("found path \"%s\"", foundpath.c_str());
          ladspapaths.push_back(foundpath);
        }

        pos = delim + 1;
      }
      else
      {
        if (pos < path.length())
        {
          string foundpath = path.substr(pos);
          LogDebug("found path \"%s\"", foundpath.c_str());
          ladspapaths.push_back(foundpath);
        }

        break;
      }
    }
  }

  bool haspaths = !ladspapaths.empty();
  if (!haspaths) //LADSPA_PATH not set or does not contain paths, use default paths
  {
    string homepath;
    if (GetHomePath(homepath))
      ladspapaths.push_back(homepath + ".ladspa");

    ladspapaths.push_back("/usr/local/lib/ladspa");
    ladspapaths.push_back("/usr/lib/ladspa");
  }

  string logstr;
  for (vector<string>::iterator it = ladspapaths.begin(); it != ladspapaths.end(); it++)
  {
    *it = PutSlashAtEnd(*it);
    logstr += " " + *it;
  }

  if (ladspaptr)
  {
    if (haspaths)
      Log("Using paths from LADSPA_PATH environment variable%s", logstr.c_str());
    else
      Log("Did not get any paths from LADSPA_PATH environment variable, using default paths%s", logstr.c_str());
  }
  else
  {
    Log("LADSPA_PATH environment variable not set, using default paths%s", logstr.c_str());
  }
}

void CBobDSP::LoadSettings()
{
  m_clientsmanager.LoadFile(false);
  m_portconnector.LoadFile(false);
  m_visualizer.LoadFile(false);
}

void CBobDSP::JackError(const char* jackerror)
{
  LogDebug("%s", jackerror);
}

void CBobDSP::JackInfo(const char* jackinfo)
{
  LogDebug("%s", jackinfo);
}

