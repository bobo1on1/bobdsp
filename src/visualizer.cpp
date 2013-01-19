/*
 * bobdsp
 * Copyright (C) Bob 2012-2013
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

#include "visualizer.h"
#include "util/misc.h"
#include "util/log.h"
#include "util/timeutils.h"
#include "util/lock.h"

using namespace std;

CVisType::CVisType(EVISTYPE type, std::string name, int outsamplerate, int bands)
{
  m_type     = type;
  if (m_type == SPECTRUM)
    m_typestr = "spectrum";
  else if (m_type == MEAN)
    m_typestr = "mean";
  else if (m_type == RMS)
    m_typestr = "rms";
  else if (m_type == PEAK)
    m_typestr = "peak";
  else
    m_typestr = "invalid";

  m_name     = name;
  m_bands    = bands;
  m_port     = NULL;
  m_srcstate = NULL;

  for (int i = 0; i < 2; i++)
  {
    m_buf[i]     = NULL;
    m_bufsize[i] = 0;
    m_bufpos[i]  = 0;
    m_buffill[i] = 0;
  }

  m_outsamplerate = outsamplerate;

  m_hasvisoutput        = false;
  m_visamplitude        = 0.0f;
  m_visamplitudesamples = 0;
}

CVisType::~CVisType()
{
  for (int i = 0; i < 2; i++)
    delete[] m_buf[i];

  if (m_srcstate)
    src_delete(m_srcstate);
}

bool CVisType::Connect(jack_client_t* client, int samplerate, int64_t interval)
{
  //allocate a buffer, twice the interval, but a minimum of 100 ms
  m_samplerate = samplerate;
  if (m_outsamplerate == -1)
    Allocate(Max((int64_t)m_samplerate / 10, interval * m_samplerate * 2 / 1000000));
  else
    Allocate(Max((int64_t)m_outsamplerate / 10, interval * m_outsamplerate * 2 / 1000000));

  m_port = jack_port_register(client, m_name.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
  if (m_port == NULL)
  {
    LogError("Error registering visualizer jack port \"%s\": \"%s\"", m_name.c_str(), GetErrno().c_str());
    return false;
  }
  else
  {
    return true;
  }
}

void CVisType::Disconnect(jack_client_t* client, bool unregister)
{
  if (m_port)
  {
    if (unregister)
    {
      int returnv = jack_port_unregister(client, m_port);
      if (returnv != 0)
        LogError("Error %i unregistering visualizer port \"%s\": \"%s\"", returnv, m_name.c_str(), GetErrno().c_str());
    }

    m_port = NULL;
  }
}

void CVisType::ProcessJack(bool locked, jack_nframes_t nframes, int64_t time)
{
  float* jackptr = (float*)jack_port_get_buffer(m_port, nframes);

  if (locked && m_buffill[1] > 0)
  {
    for (int i = 0; i < 2; i++)
    {
      int copy = Min(m_buffill[1], m_bufsize[1] - m_bufpos[1]);
      Append(0, m_buf[1] + m_bufpos[1], copy, m_buftime[1]);
      m_buffill[1] -= copy;
      m_bufpos[1] += copy;
      if (m_bufpos[1] >= m_bufsize[1])
        m_bufpos[1] -= m_bufsize[1];
    }
  }

  if (m_outsamplerate == -1)
    Append(locked ? 0 : 1, jackptr, nframes, time);
  else
    Resample(locked ? 0 : 1, jackptr, nframes, time);
}

void CVisType::Allocate(int bufsize)
{ 
  //make sure the buffer is at least one byte big
  bufsize = Max(bufsize, 1);

  for (int bufindex = 0; bufindex < 2; bufindex++)
  {
    //reallocate the buffer if the size changed
    if (bufsize != m_bufsize[bufindex])
    {
      m_bufsize[bufindex] = bufsize; 
      delete[] m_buf[bufindex];
      m_buf[bufindex] = new float[m_bufsize[bufindex]];
    }

    //reset the buffer
    m_bufpos[bufindex]  = 0;
    m_buffill[bufindex] = 0;
    memset(m_buf[bufindex], 0, m_bufsize[bufindex] * sizeof(float));
  }

  if (m_outsamplerate != -1)
  {
    if (m_srcstate)
      src_delete(m_srcstate);

    //allocate a new libsamplerate resampler
    int error;
    m_srcstate = src_new(SRC_SINC_FASTEST, 1, &error);
  }
}

void CVisType::Append(int bufindex, float* data, int samples, int64_t time)
{
  //if this is the first sample going into the ringbuffer, store the timestamp
  if (m_buffill[bufindex] == 0)
    m_buftime[bufindex] = time;

  int totalcopy = Min(samples, m_bufsize[bufindex] - m_buffill[bufindex]);
  if (totalcopy == 0)
    return; //nothing to copy

  //calculate where to start writing in the ringbuffer
  int writepos = m_bufpos[bufindex] + m_buffill[bufindex];
  if (writepos >= m_bufsize[bufindex])
    writepos -= m_bufsize[bufindex];

  //copy the first part, making sure nothing is written beyond the buffer
  int copy = Min(totalcopy, m_bufsize[bufindex] - writepos);
  memcpy(m_buf[bufindex] + writepos, data, copy * sizeof(float));

  //if there's any data left, copy it in at the beginning of the buffer
  if (totalcopy > copy)
    memcpy(m_buf[bufindex], data + copy, (totalcopy - copy) * sizeof(float));

  m_buffill[bufindex] += totalcopy;
}

void CVisType::Resample(int bufindex, float* data, int samples, int64_t time)
{
  //if this is the first sample going into the ringbuffer, store the timestamp
  if (m_buffill[bufindex] == 0)
    m_buftime[bufindex] = time;

  SRC_DATA srcdata = {};
  srcdata.src_ratio = (double)m_outsamplerate / m_samplerate;
  srcdata.data_in = data;
  srcdata.input_frames = samples;

  for (int i = 0; i < 2; i++)
  {
    //calculate where to start writing in the ringbuffer
    int writepos = m_bufpos[bufindex] + m_buffill[bufindex];
    if (writepos >= m_bufsize[bufindex])
      writepos -= m_bufsize[bufindex];

    int outbufspace = m_bufsize[bufindex] - m_buffill[bufindex];
    srcdata.data_out = m_buf[bufindex] + writepos;
    srcdata.output_frames = Min(outbufspace, m_bufsize[bufindex] - writepos);

    src_process(m_srcstate, &srcdata);

    m_buffill[bufindex] += srcdata.output_frames_gen;

    srcdata.data_in += srcdata.input_frames_used;
    srcdata.input_frames -= srcdata.input_frames_used;
  }
}

void CVisType::ProcessVisualizer(int64_t vistime)
{
  if (m_type == SPECTRUM)
    ProcessSpectrum(vistime);
  else if (m_type == MEAN)
    ProcessAverage(vistime, Abs);
  else if (m_type == RMS)
    ProcessAverage(vistime, Square);
  else if (m_type == PEAK)
    ProcessAverage(vistime, Peak);
}

void CVisType::ProcessSpectrum(int64_t vistime)
{
}

void CVisType::ProcessAverage(int64_t vistime, void(*processfunc)(float, float&, int&))
{
  int samplesforvis = (vistime - m_buftime[0]) * m_samplerate / 1000000 + 1;
  int samples = Min(samplesforvis, m_buffill[0]);

  for (int i = 0, pos = m_bufpos[0]; i < samples; i++)
  {
    processfunc(m_buf[0][pos], m_visamplitude, m_visamplitudesamples);
    pos++;
    if (pos >= m_bufpos[0])
      pos = 0;
  }

  m_buftime[0] += (int64_t)samples * 1000000 / m_samplerate;
  m_buffill[0] -= samples;
  m_bufpos[0] += samples;
  if (m_bufpos[0] >= m_bufsize[0])
    m_bufpos[0] -= m_bufsize[0];

  if (vistime <= m_buftime[0])
    m_hasvisoutput = true;
}

void CVisType::Square(float in, float& avg, int& samples)
{
  avg += in * in;
  samples++;
}

void CVisType::Abs(float in, float& avg, int& samples)
{
  avg += fabsf(in);
  samples++;
}

void CVisType::Peak(float in, float& highest, int& samples)
{
  float absin = fabsf(in);
  if (absin > highest)
    highest = absin;

  samples = 1;
}

void CVisType::ClearVisOutput()
{
  m_hasvisoutput        = false;
  m_visamplitude        = 0.0f;
  m_visamplitudesamples = 0;
}

void CVisType::ToJSON(JSON::CJSONGenerator& generator)
{
  generator.MapOpen();

  generator.AddString("name");
  generator.AddString(m_name);
  generator.AddString("type");
  generator.AddString(m_typestr);

  if (m_type == MEAN || m_type == RMS || m_type == PEAK)
  {
    generator.AddString("value");
    float value;
    if (m_visamplitudesamples > 0)
      value = m_visamplitude / m_visamplitudesamples;
    else
      value = 0.0f;
    generator.AddDouble(value);
  }

  generator.MapClose();
}

CVisualizer::CVisualizer()
{
  m_client       = NULL;
  m_exitstatus   = (jack_status_t)0;
  m_connected    = false;
  m_wasconnected = true;
  m_samplerate   = 0;
  m_vistime      = 0;
  m_index        = 0;

  VisualizersToJSON();
}

CVisualizer::~CVisualizer()
{
}

void CVisualizer::VisualizersFromXML(TiXmlElement* root)
{
  bool loadfailed = false;

  LOADINTELEMENT(root, interval, MANDATORY, 1, POSTCHECK_ONEORHIGHER);
  if (loadfailed)
    return;

  m_interval = interval_p;

  for (TiXmlElement* vis = root->FirstChildElement(); vis != NULL; vis = vis->NextSiblingElement())
  {
    if (vis->ValueStr() != "visualizer")
      continue;

    LogDebug("Read <%s> element", vis->Value());

    LOADSTRELEMENT(vis, name, MANDATORY);
    LOADSTRELEMENT(vis, type, MANDATORY);

    if (loadfailed)
      continue;

    EVISTYPE vistype;
    if (strcmp(type->GetText(), "spectrum") == 0)
      vistype = SPECTRUM;
    else if (strcmp(type->GetText(), "mean") == 0)
      vistype = MEAN;
    else if (strcmp(type->GetText(), "rms") == 0)
      vistype = RMS;
    else if (strcmp(type->GetText(), "peak") == 0)
      vistype = PEAK;
    else
    {
      LogError("Invalid visualizer type \"%s\" for visualizer \"%s\"", type->GetText(), name->GetText());
      continue;
    }

    int nrbands = 0;
    if (vistype == SPECTRUM)
    {
      LOADINTELEMENT(vis, bands, MANDATORY, 1, POSTCHECK_ONEORHIGHER);
      if (loadfailed)
        continue;

      nrbands = bands_p;
    }

    if (vistype == SPECTRUM)
      LogDebug("Visualizer name:\"%s\" type:\"%s\" bands:%i", name->GetText(), type->GetText(), nrbands);
    else
      LogDebug("Visualizer name:\"%s\" type:\"%s\"", name->GetText(), type->GetText());

    int outsamplerate;
    if (vistype == SPECTRUM)
      outsamplerate = 40000;
    else
      outsamplerate = -1;

    m_visualizers.push_back(CVisType(vistype, name->GetText(), outsamplerate, nrbands));
  }
}

void CVisualizer::Start()
{
  if (m_visualizers.empty())
  {
    LogDebug("No visualizers configured");
  }
  else
  {
    LogDebug("Starting visualizer thread");
    StartThread();
  }
}

void CVisualizer::Process()
{
  while(!m_stop)
  {
    m_connected = Connect();
    if (!m_connected)
    {
      Disconnect(true);
      USleep(1000000);
      continue;
    }
    else
    {
      m_wasconnected = true;
    }

    CLock lock(m_jackcond);
    m_jackcond.Wait(1000000);

    if (m_exitstatus)
    {
      LogError("Visualizer exited with code %i reason: \"%s\"", m_exitstatus, m_exitreason.c_str());
      Disconnect(false);
      continue;
    }

    bool allhaveoutput = true;
    for (vector<CVisType>::iterator it = m_visualizers.begin(); it != m_visualizers.end(); it++)
    {
      it->ProcessVisualizer(m_vistime);
      if (!it->HasVisOutput())
        allhaveoutput = false;
    }

    lock.Leave();

    if (allhaveoutput)
    {
      m_vistime += m_interval;
      uint64_t now = GetTimeUs();
      USleep(m_vistime - now);
      VisualizersToJSON();
      m_viscond.Signal();
    }
  }

  Disconnect(true);
}

std::string CVisualizer::JSON()
{
  CLock lock(m_viscond);
  return m_json;
}

std::string CVisualizer::JSON(const std::string& postjson)
{
  TiXmlElement* root = JSON::JSONToXML(postjson);
  bool loadfailed = false;

  LOADINTELEMENT(root, timeout, OPTIONAL, 0, POSTCHECK_ZEROORHIGHER);
  LOADINTELEMENT(root, index, OPTIONAL, -1, POSTCHECK_NONE);

  delete root;

  CLock lock(m_viscond);

  //wait for the port index to change with the client requested timeout
  //the maximum timeout is one minute
  if (index_p == (int64_t)m_index && timeout_p > 0 && !m_stop)
    m_viscond.Wait(Min(timeout_p, 60000) * 1000, m_index, (unsigned int)index_p);

  return m_json;
}

bool CVisualizer::Connect()
{
  if (m_connected)
    return true;

  LogDebug("Connecting visualizer to jack");

  //this is set in PJackInfoShutdownCallback(), init to 0 here so we know when the jack thread has exited
  m_exitstatus = (jack_status_t)0; 
  m_exitreason.clear();

  //try to connect to jackd
  m_client = jack_client_open("BobDSP Visualizer", JackNoStartServer, NULL);
  if (m_client == NULL)
  {
    if (m_wasconnected || g_printdebuglevel)
    {
      LogError("Error connecting visualizer to jackd: \"%s\"", GetErrno().c_str());
      m_wasconnected = false; //only print this to the log once
    }
    return false;
  }

  //we want to know when the jack thread shuts down, so we can restart it
  jack_on_info_shutdown(m_client, SJackInfoShutdownCallback, this);

  m_samplerate = jack_get_sample_rate(m_client);

  Log("Visualizer connected to jackd, got name \"%s\", samplerate %i", jack_get_client_name(m_client), m_samplerate);

  //SJackProcessCallback gets called when jack has new audio data to process
  int returnv = jack_set_process_callback(m_client, SJackProcessCallback, this);
  if (returnv != 0)
  {
    LogError("Error %i setting visualizer process callback: \"%s\"", returnv, GetErrno().c_str());
    return false;
  }

  //register a jack port for each visualizer
  for (vector<CVisType>::iterator it = m_visualizers.begin(); it != m_visualizers.end(); it++)
  {
    if (!it->Connect(m_client, m_samplerate, m_interval))
      return false;
  }

  //everything set up, activate
  returnv = jack_activate(m_client);
  if (returnv != 0)
  {
    LogError("Error %i activating visualizer: \"%s\"", returnv, GetErrno().c_str());
    return false;
  }

  //initialize the visualizer time
  m_vistime = GetTimeUs() + m_interval;

  return true;
}

void CVisualizer::Disconnect(bool unregisterjack)
{
  m_connected = false;

  if (m_client)
  {
    //deactivate the client
    int returnv = jack_deactivate(m_client);
    if (returnv != 0)
      LogError("Error %i deactivating visualizer: \"%s\"", returnv, GetErrno().c_str());

    //unregister ports, unless the jack thread exited because jackd stopped, since this might hang in libjack
    if (unregisterjack)
    {
      for (vector<CVisType>::iterator it = m_visualizers.begin(); it != m_visualizers.end(); it++)
        it->Disconnect(m_client, unregisterjack);
    }

    //destroy the jack client
    returnv = jack_client_close(m_client);
    if (returnv != 0)
      LogError("Error %i closing visualizer: \"%s\"", returnv, GetErrno().c_str());

    m_client = NULL;
  }
}

void CVisualizer::VisualizersToJSON()
{
  JSON::CJSONGenerator generator;

  generator.MapOpen();

  generator.AddString("visualizers");
  generator.ArrayOpen();
  for (vector<CVisType>::iterator it = m_visualizers.begin(); it != m_visualizers.end(); it++)
  {
    it->ToJSON(generator);
    it->ClearVisOutput();
  }
  generator.ArrayClose();

  CLock lock(m_viscond);

  generator.AddString("index");
  generator.AddInt(++m_index);
  generator.MapClose();

  generator.ToString(m_json);
}

int CVisualizer::SJackProcessCallback(jack_nframes_t nframes, void *arg)
{
  ((CVisualizer*)arg)->PJackProcessCallback(nframes);
  return 0;
}

void CVisualizer::PJackProcessCallback(jack_nframes_t nframes)
{
  //get a timestamp of the first sample in the buffer
  int64_t now = GetTimeUs();

  //this function is called from a realtime thread, blocking on a mutex here is very bad,
  //so try to lock the mutex, if it fails (because it's already locked), store the samples in a ringbuffer
  //they will be processed during the next iteration
  CLock lock(m_jackcond, true);

  for (vector<CVisType>::iterator it = m_visualizers.begin(); it != m_visualizers.end(); it++)
    it->ProcessJack(lock.HasLock(), nframes, now);

  if (lock.HasLock())
    m_jackcond.Signal();
}

void CVisualizer::SJackInfoShutdownCallback(jack_status_t code, const char *reason, void *arg)
{
  ((CVisualizer*)arg)->PJackInfoShutdownCallback(code, reason);
}

void CVisualizer::PJackInfoShutdownCallback(jack_status_t code, const char *reason)
{
  CLock lock(m_jackcond);
  m_exitreason = reason;
  m_exitstatus = code;
  m_jackcond.Signal();
}

