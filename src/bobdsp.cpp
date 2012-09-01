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

#include "util/log.h"
#include "util/misc.h"
#include "util/timeutils.h"

#define CONNECTINTERVAL 10000000

using namespace std;

CBobDSP::CBobDSP(int argc, char *argv[]):
  m_portconnector(*this),
  m_httpserver(*this)
{
  m_stop     = false;
  m_signalfd = -1;
  m_stdout[0] = m_stdout[1] = -1;
  m_stderr[0] = m_stderr[1] = -1;

  bool dofork = false;

  //parse commandline options
  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0)
    {
      g_printdebuglevel = true;
    }
    else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--fork") == 0)
    {
      dofork = true;
    }
    else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0)
    {
      if (i == argc - 1)
      {
        printf("Error: option %s requires an argument\n", argv[i]);
        exit(1);
      }

      i++;
      int port;
      if (!StrToInt(argv[i], port) || port < 0 || port > 65535)
      {
        printf("Error: invalid argument %s for option %s\n", argv[i], argv[i - 1]);
        exit(1);
      }

      m_httpserver.SetPort(port);
    }
    else
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
             "\n"
             );
      exit(0);
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

void CBobDSP::Setup()
{
  //set up locked memory for better realtime performance
  SetupRT(1024 * 1024 * 10); //10 mb

  //init the logfile
  SetLogFile("bobdsp.log");

  //get paths to load ladspa plugins from
  vector<string> ladspapaths;
  LoadLadspaPaths(ladspapaths);

  //load ladspa plugins
  for (vector<string>::iterator it = ladspapaths.begin(); it != ladspapaths.end(); it++)
    CLadspaPlugin::GetPlugins(*it, m_plugins);

  Log("Found %zu plugins", m_plugins.size());
  for (vector<CLadspaPlugin*>::iterator it = m_plugins.begin(); it != m_plugins.end(); it++)
  {
    const LADSPA_Descriptor* d = (*it)->Descriptor();
    LogDebug("ID:%lu Label:\"%s\" Name:\"%s\" Ports:%lu", d->UniqueID, d->Label, d->Name, d->PortCount);
  }

  //load the plugins we want to use
  LoadPluginsFromFile();

  //load the port connections
  LoadConnectionsFromFile();

  //set up signal handlers
  SetupSignals();
}

void CBobDSP::Process()
{
  Log("Starting %zu jack client(s)", m_clients.size());

  //set up timestamp so we connect on the first iteration
  int64_t lastconnect = GetTimeUs() - CONNECTINTERVAL;
  bool checkconnect = false;
  bool checkdisconnect = false;
  while(!m_stop)
  {
    bool triedconnect = false;
    bool allconnected = true;

    //check if we need to start the http server
    if (!m_httpserver.IsStarted())
    {
      allconnected = false; //use 10 second timeout in ProcessMessages()
      if (GetTimeUs() - lastconnect >= CONNECTINTERVAL)
      {
        m_httpserver.Start();
        triedconnect = true; //update timestamp
      }
    }

    for (vector<CJackClient*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
    {
      //check if the jack thread has failed
      if ((*it)->ExitStatus())
      {
        LogError("Client \"%s\" exited with code %i reason: \"%s\"",
                 (*it)->Name().c_str(), (*it)->ExitStatus(), (*it)->ExitReason().c_str());
        (*it)->Disconnect();
      }

      //keep trying to connect
      //only try to connect every 10 seconds to prevent hammering jackd
      if (!(*it)->IsConnected())
      {
        allconnected = false;
        if (GetTimeUs() - lastconnect >= CONNECTINTERVAL)
        {
          triedconnect = true;
          if ((*it)->Connect())
            checkconnect = true;
        }
      }
    }

    if (triedconnect)
      lastconnect = GetTimeUs();

    //if a client connected, or a port callback was called
    //process port connections, if it fails try again in 10 seconds
    m_portconnector.Process(checkconnect, checkdisconnect);

    //process messages, blocks if there's nothing to do
    ProcessMessages(checkconnect, checkdisconnect, !allconnected || checkconnect || checkdisconnect);

    LogDebug("main loop woke up");
  }
}

void CBobDSP::Cleanup()
{
  Log("Stopping %zu jack client(s)", m_clients.size());
  for (vector<CJackClient*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
    delete *it;

  for (vector<CLadspaPlugin*>::iterator it = m_plugins.begin(); it != m_plugins.end(); it++)
    delete *it;

  if (m_signalfd != -1)
    close(m_signalfd);

  m_httpserver.Stop();
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

void CBobDSP::ProcessMessages(bool& checkconnect, bool& checkdisconnect, bool usetimeout)
{
  unsigned int nrfds = 0;
  pollfd* fds        = new pollfd[m_clients.size() + 4];

  for (vector<CJackClient*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
  {
    int pipe = (*it)->MsgPipe();
    if (pipe != -1)
    {
      nrfds++;
      fds[nrfds - 1].fd     = pipe;
      fds[nrfds - 1].events = POLLIN;
    }
  }

  int pipes[4] = { m_stdout[0], m_stderr[0], m_signalfd, m_httpserver.MsgPipe() };
  int pipenrs[4] = { -1, -1, -1, -1 };

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

  if (nrfds == 0)
  {
    LogDebug("no file descriptors to wait on");
    delete[] fds;
    sleep(1);
    return;
  }
  else
  {
    LogDebug("Waiting on %i file descriptors", nrfds);
  }

  int timeout;
  if (usetimeout)
    timeout = 10000; //return after 10 seconds to reconnect clients
  else
    timeout = -1; //don't have to reconnect clients, infinite wait

  int returnv = poll(fds, nrfds, timeout);
  if (returnv == -1)
  {
    LogError("poll on msg pipes: %s", GetErrno().c_str());
    sleep(1); //to prevent busy spinning in case we don't handle the error correctly
  }
  else if (returnv > 0)
  {
    //check client messages
    ProcessClientMessages(checkconnect, checkdisconnect);
    
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
      ProcessHttpServerMessages(checkconnect, checkdisconnect);
  }

  delete[] fds;
}

void CBobDSP::ProcessClientMessages(bool& checkconnect, bool& checkdisconnect)
{
  //wait 1 ms for other events to come in, since in case of a port event
  //we get an event from every client
  USleep(1000);

  //check events of all clients
  for (vector<CJackClient*>::iterator it = m_clients.begin(); it != m_clients.end(); it++)
  {
    uint8_t msg;
    while ((msg = (*it)->GetMessage()) != MsgNone)
    {
      LogDebug("got message %s from client \"%s\"", MsgToString(msg), (*it)->Name().c_str());
      if (msg == MsgPortRegistered || msg == MsgPortDisconnected)
        checkconnect = true;
      else if (msg == MsgPortConnected)
        checkdisconnect = true;
    }
  }
}

void CBobDSP::ProcessSignalfd()
{
  signalfd_siginfo siginfo;
  int returnv = read(m_signalfd, &siginfo, sizeof(siginfo));
  if (returnv == -1 && errno != EAGAIN)
  {
    LogError("reading signals fd: %s", GetErrno().c_str());
    if (errno != EINTR)
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

  if (!logstr.empty())
    LogDebug("%s: %s", name, logstr.c_str());

  if (returnv == -1 && errno != EAGAIN)
  {
    LogError("reading %s fd: \"%s\"", name, GetErrno().c_str());
    if (errno != EINTR)
    {
      close(fd);
      fd = -1;
    }
  }
}

void CBobDSP::ProcessHttpServerMessages(bool& checkconnect, bool& checkdisconnect)
{
  uint8_t msg;
  while ((msg = m_httpserver.GetMessage()) != MsgNone)
  {
    LogDebug("got message %s from httpserver", MsgToString(msg));
    if (msg == MsgConnectionsUpdated)
      checkconnect = checkdisconnect = true;
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

bool CBobDSP::LoadPluginsFromFile()
{
  string homepath;
  if (!GetHomePath(homepath))
  {
    LogError("Unable to get home path");
    return false;
  }

  string filename = homepath + ".bobdsp/plugins.xml";
  Log("Loading plugin settings from %s", filename.c_str());

  TiXmlDocument pluginsfile;
  pluginsfile.LoadFile(filename.c_str());

  if (pluginsfile.Error())
  {
    LogError("Unable to load %s: %s %s %s", filename.c_str(), pluginsfile.ErrorDesc(),
        pluginsfile.ErrorRow() ? (string("Row: ") + ToString(pluginsfile.ErrorRow())).c_str() : "",
        pluginsfile.ErrorCol() ? (string("Col: ") + ToString(pluginsfile.ErrorCol())).c_str() : "");
    return false;
  }

  TiXmlElement* root = pluginsfile.RootElement();
  if (!root)
  {
    LogError("Unable to get <plugins> root node from %s", filename.c_str());
    return false;
  }

  LoadPluginsFromRoot(root);

  return true;
}

/* load plugin settings from plugins.xml
   it may look like this:

<plugins>
  <clientprefix>bobdsp_</clientprefix>
  <portprefix>ladspa_</portprefix>
  <plugin>
    <name>Audio limiter</name>
    <label>fastLookaheadLimiter</label>
    <uniqueid>1913</uniqueid>
    <instances>1</instances>
    <pregain>1.0</pregain>
    <postgain>1.0</postgain>
    <portprefix>lim_</portprefix>
    <port>
      <name>Input gain (dB)</name>
      <value>0</value>
    </port>
    <port>
      <name>Limit (dB)</name>
      <value>0</value>
    </port>
    <port>
      <name>Release time (s)</name>
      <value>0.1</value>
    </port>
  </plugin>
</plugins>

  <clientprefix>bobdsp_</clientprefix> is a string prefixed to every client name
  <portprefix>ladspa_</portprefix> is a string prefixed to every port name
  <portprefix>lim_</portprefix> is a string prefixed to every port name of the plugin
  <name> is the name for the jack client, it's not related to the name of the ladspa plugin
  <label> is the label of the ladspa plugin
  <uniqueid> is the unique id of the ladspa plugin
  <instances> sets the number of instances for the plugin, by increasing instances you can process more audio channels
  <pregain> is the audio gain for the ladspa input ports
  <postgain> is the audio gain for the ladspa output ports
*/

void CBobDSP::LoadPluginsFromRoot(TiXmlElement* root)
{
  TiXmlElement* gclientprefix = root->FirstChildElement("clientprefix");
  TiXmlElement* gportprefix = root->FirstChildElement("portprefix");

  for (TiXmlElement* plugin = root->FirstChildElement("plugin"); plugin != NULL; plugin = plugin->NextSiblingElement("plugin"))
  {
    LogDebug("Read <plugin> element");

    bool loadfailed = false;

    LOADELEMENT(plugin, name, MANDATORY);
    LOADELEMENT(plugin, label, MANDATORY);
    LOADELEMENT(plugin, portprefix, OPTIONAL);
    LOADINTELEMENT(plugin, uniqueid, MANDATORY, 0, POSTCHECK_NONE);
    LOADINTELEMENT(plugin, instances, OPTIONAL, 1, POSTCHECK_ONEORHIGHER);
    LOADFLOATELEMENT(plugin, pregain, OPTIONAL, 1.0, POSTCHECK_NONE);
    LOADFLOATELEMENT(plugin, postgain, OPTIONAL, 1.0, POSTCHECK_NONE);

    if (loadfailed || instances_parsefailed || pregain_parsefailed || postgain_parsefailed)
      continue;

    LogDebug("name:\"%s\" label:\"%s\" uniqueid:%" PRIi64 " instances:%" PRIi64 " pregain:%.2f postgain:%.2f",
             name->GetText(), label->GetText(), uniqueid_p, instances_p, pregain_p, postgain_p);

    vector<portvalue> portvalues;
    if (!LoadPortsFromPlugin(plugin, portvalues))
      continue;

    CLadspaPlugin* ladspaplugin = SearchLadspaPlugin(m_plugins, uniqueid_p, label->GetText());
    if (ladspaplugin)
    {
      LogDebug("Found matching ladspa plugin in %s", ladspaplugin->FileName().c_str());
    }
    else
    {
      LogError("Did not find matching ladspa plugin for \"%s\" label \"%s\" uniqueid %" PRIi64,
               name->GetText(), label->GetText(), uniqueid_p);
      continue;
    }

    //check if all ports from the xml match the ladspa plugin
    bool allportsok = true;
    for (vector<portvalue>::iterator it = portvalues.begin(); it != portvalues.end(); it++)
    {
      bool found = false;
      for (unsigned long port = 0; port < ladspaplugin->PortCount(); port++)
      {
        LADSPA_PortDescriptor p = ladspaplugin->PortDescriptor(port);
        if (LADSPA_IS_PORT_INPUT(p) && LADSPA_IS_PORT_CONTROL(p) && it->first == ladspaplugin->PortName(port))
        {
          LogDebug("Found port \"%s\"", it->first.c_str());
          found = true;
          break;
        }
      }
      if (!found)
      {
        LogError("Did not find port \"%s\" in plugin \"%s\"", it->first.c_str(), ladspaplugin->Label());
        allportsok = false;
      }
    }

    //check if all control input ports are mapped
    for (unsigned long port = 0; port < ladspaplugin->PortCount(); port++)
    {
      LADSPA_PortDescriptor p = ladspaplugin->PortDescriptor(port);
      if (LADSPA_IS_PORT_INPUT(p) && LADSPA_IS_PORT_CONTROL(p))
      {
        bool found = false;
        for (vector<portvalue>::iterator it = portvalues.begin(); it != portvalues.end(); it++)
        {
          if (LADSPA_IS_PORT_INPUT(p) && LADSPA_IS_PORT_CONTROL(p) && it->first == ladspaplugin->PortName(port))
          {
            found = true;
            break;
          }
        }
        if (!found)
        {
          LogError("Port \"%s\" of plugin \"%s\" is not mapped", ladspaplugin->PortName(port), ladspaplugin->Label());
          allportsok = false;
        }
      }

      if (!PortDescriptorSanityCheck(ladspaplugin, port, p))
        allportsok = false;
    }

    if (!allportsok)
      continue;

    string strclientprefix;
    if (gclientprefix && gclientprefix->GetText())
      strclientprefix = gclientprefix->GetText();

    string strportprefix;
    if (gportprefix && gportprefix->GetText())
      strportprefix = gportprefix->GetText();
    if (!portprefix_loadfailed)
      strportprefix += portprefix->GetText();

    CJackClient* jackclient = new CJackClient(ladspaplugin, name->GetText(), instances_p, pregain_p,
                                              postgain_p, portvalues, strclientprefix, strportprefix);
    m_clients.push_back(jackclient);
  }
}

bool CBobDSP::LoadPortsFromPlugin(TiXmlElement* plugin, std::vector<portvalue>& portvalues)
{
  bool success = true;

  for (TiXmlElement* port = plugin->FirstChildElement("port"); port != NULL; port = port->NextSiblingElement("port"))
  {
    LogDebug("Read <port> element");

    bool loadfailed = false;

    LOADELEMENT(port, name, MANDATORY);
    LOADFLOATELEMENT(port, value, MANDATORY, 0, POSTCHECK_NONE);

    if (loadfailed)
    {
      success = false;
      continue;
    }

    LogDebug("name:\"%s\" value:%.2f", name->GetText(), value_p);
    portvalues.push_back(make_pair(name->GetText(), value_p));
  }

  return success;
}

CLadspaPlugin* CBobDSP::SearchLadspaPlugin(vector<CLadspaPlugin*> plugins, int64_t uniqueid, const char* label)
{
  CLadspaPlugin* ladspaplugin = NULL;
  for (vector<CLadspaPlugin*>::iterator it = plugins.begin(); it != plugins.end(); it++)
  {
    if (uniqueid == (int64_t)(*it)->UniqueID() && strcmp((*it)->Label(), label) == 0)
    {
      ladspaplugin = (*it);
      break;
    }
  }

  return ladspaplugin;
}

bool CBobDSP::PortDescriptorSanityCheck(CLadspaPlugin* plugin, unsigned long port, LADSPA_PortDescriptor p)
{
  bool isinput = LADSPA_IS_PORT_INPUT(p);
  bool isoutput = LADSPA_IS_PORT_OUTPUT(p);
  bool iscontrol = LADSPA_IS_PORT_CONTROL(p);
  bool isaudio = LADSPA_IS_PORT_AUDIO(p);

  bool isok = true;

  if (!isinput && !isoutput)
  {
    LogError("Port \"%s\" of plugin %s does not have input or output flag set", plugin->PortName(port), plugin->Label());
    isok = false;
  }
  else if (isinput && isoutput)
  {
    LogError("Port \"%s\" of plugin %s has both input and output flag set", plugin->PortName(port), plugin->Label());
    isok = false;
  }

  if (!iscontrol && !isaudio)
  {
    LogError("Port \"%s\" of plugin %s does not have audio or control flag set", plugin->PortName(port), plugin->Label());
    isok = false;
  }
  else if (iscontrol && isaudio)
  {
    LogError("Port \"%s\" of plugin %s has both audio and control flag set", plugin->PortName(port), plugin->Label());
    isok = false;
  }

  return isok;
}

#define CONNECTIONSFILE "connections.xml"

bool CBobDSP::LoadConnectionsFromFile()
{
  string homepath;
  if (!GetHomePath(homepath))
  {
    LogError("Unable to get home path");
    return false;
  }

  string filename = homepath + ".bobdsp/" + CONNECTIONSFILE;
  Log("Loading connection settings from %s", filename.c_str());

  TiXmlDocument connectionsfile;
  connectionsfile.LoadFile(filename.c_str());

  if (connectionsfile.Error())
  {
    LogError("Unable to load %s: %s %s %s", filename.c_str(), connectionsfile.ErrorDesc(),
        connectionsfile.ErrorRow() ? (string("Row: ") + ToString(connectionsfile.ErrorRow())).c_str() : "",
        connectionsfile.ErrorCol() ? (string("Col: ") + ToString(connectionsfile.ErrorCol())).c_str() : "");
    return false;
  }

  TiXmlElement* root = connectionsfile.RootElement();
  if (!root)
  {
    LogError("Unable to get <connections> root node from %s", filename.c_str());
    return false;
  }

  return m_portconnector.ConnectionsFromXML(root);
}

bool CBobDSP::SaveConnectionsToFile(TiXmlElement* connections)
{
  TiXmlDocument doc;
  doc.LinkEndChild(connections);

  string homepath;
  if (!GetHomePath(homepath))
  {
    LogError("Unable to get home path");
    return false;
  }

  string filename = homepath + ".bobdsp/" + CONNECTIONSFILE;
  Log("Saving connection settings to %s", filename.c_str());

  if (!doc.SaveFile(filename))
  {
    LogError("Error saving connections: \"%s\"", doc.ErrorDesc());
    return false;
  }

  return true;
}

