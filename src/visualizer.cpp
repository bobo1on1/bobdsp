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
#include "util/thread.h"

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

void CVisType::ToJSON(CJSONGenerator& generator, bool values)
{
  generator.MapOpen();

  generator.AddString("name");
  generator.AddString(m_name);
  generator.AddString("type");
  generator.AddString(m_typestr);

  if (values && (m_type == MEAN || m_type == RMS || m_type == PEAK))
  {
    generator.AddString("value");
    float value;
    if (m_visamplitudesamples > 0)
      value = m_visamplitude / m_visamplitudesamples;
    else
      value = 0.0f;
    generator.AddDouble(value);
  }

  if (m_type == SPECTRUM)
  {
    generator.AddString("bands");
    generator.AddInt(m_bands);
  }

  generator.MapClose();
}

CVisualizer::CVisualizer() :
  CJSONSettings(".bobdsp/visualizers.json", "visualizer", m_viscond)
{
  m_client       = NULL;
  m_exitstatus   = (jack_status_t)0;
  m_connected    = false;
  m_wasconnected = true;
  m_samplerate   = 0;
  m_vistime      = 0;
  m_index        = 0;
  m_interval     = 100000;

  CJSONGenerator* generator = VisualizersToJSON(true);
  generator->ToString(m_json);
  delete generator;
}

CVisualizer::~CVisualizer()
{
}

void CVisualizer::LoadSettings(JSONMap& root, bool reload, bool allowreload, const std::string& source)
{
  JSONMap::iterator visualizers = root.find("visualizers");
  if (visualizers != root.end() && !visualizers->second->IsArray())
  {
    LogError("%s: invalid value for visualizers: %s", source.c_str(), ToJSON(visualizers->second).c_str());
    return;
  }

  JSONMap::iterator interval = root.find("interval");
  if (interval != root.end() && (!interval->second->IsNumber() || interval->second->ToInt64() <= 0))
  {
    LogError("%s: invalid value for interval: %s", source.c_str(), ToJSON(interval->second).c_str());
    return;
  }

  JSONMap::iterator action = root.find("action");
  if (action != root.end() && !action->second->IsString())
  {
    LogError("%s: invalid value for action: %s", source.c_str(), ToJSON(action->second).c_str());
    return;
  }

  if (visualizers != root.end())
  {
    if (!LoadVisualizers(visualizers->second->AsArray(), source))
      return;
  }

  if (interval != root.end())
    m_interval = interval->second->ToInt64();
}

CJSONGenerator* CVisualizer::SettingsToJSON(bool tofile)
{
  return VisualizersToJSON(false);
}

bool CVisualizer::LoadVisualizers(JSONArray& jsonvisualizers, const std::string& source)
{
  vector<CVisType> visualizers;

  for (JSONArray::iterator it = jsonvisualizers.begin(); it != jsonvisualizers.end(); it++)
  {
    if (!(*it)->IsMap())
    {
      LogError("%s: invalid value for visualizer: %s", source.c_str(), ToJSON(*it).c_str());
      return false;
    }

    JSONMap::iterator name = (*it)->AsMap().find("name");
    if (name == (*it)->AsMap().end())
    {
      LogError("%s: visualizer has no name", source.c_str());
      return false;
    }
    else if (!name->second->IsString())
    {
      LogError("%s: invalid value for name: %s", source.c_str(), ToJSON(name->second).c_str());
      return false;
    }

    JSONMap::iterator type = (*it)->AsMap().find("type");
    if (type == (*it)->AsMap().end())
    {
      LogError("%s: visualizer %s has no type", name->second->AsString().c_str(), source.c_str());
      return false;
    }
    else if (!type->second->IsString())
    {
      LogError("%s: visualizer %s invalid value for type: %s", source.c_str(),
               name->second->AsString().c_str(), ToJSON(type->second).c_str());
      return false;
    }

    EVISTYPE vistype;
    if (type->second->AsString() == "spectrum" && false) //spectrum is unsupported right now
      vistype = SPECTRUM;
    else if (type->second->AsString() == "mean")
      vistype = MEAN;
    else if (type->second->AsString() == "rms")
      vistype = RMS;
    else if (type->second->AsString() == "peak")
      vistype = PEAK;
    else
    {
      LogError("%s: invalid visualizer type %s for visualizer %s", source.c_str(),
               type->second->AsString().c_str(), name->second->AsString().c_str());
      return false;
    }

    int64_t nrbands = 0;
    if (vistype == SPECTRUM)
    {
      JSONMap::iterator bands = (*it)->AsMap().find("bands");
      if (bands == (*it)->AsMap().end())
      {
        LogError("%s: visualizer %s has no bands", name->second->AsString().c_str(), source.c_str());
        return false;
      }
      else if (!bands->second->IsNumber() || bands->second->ToInt64() <= 0)
      {
        LogError("%s: visualizer %s invalid value for bands: %s", source.c_str(),
                 name->second->AsString().c_str(), ToJSON(bands->second).c_str());
        return false;
      }

      nrbands = bands->second->ToInt64();
    }

    int outsamplerate;
    if (vistype == SPECTRUM)
      outsamplerate = 40000;
    else
      outsamplerate = -1;

    visualizers.push_back(CVisType(vistype, name->second->AsString(), outsamplerate, nrbands));
    Log("Loaded visualizer %s type %s", visualizers.back().Name().c_str(), visualizers.back().TypeStr());
  }

  m_visualizers.swap(visualizers);

  return true;
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
  SetThreadName("visualizer");

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
      CJSONGenerator* generator = VisualizersToJSON(true);

      m_vistime += m_interval;
      uint64_t now = GetTimeUs();
      USleep(m_vistime - now);

      CLock lock(m_viscond);

      generator->ToString(m_json);
      delete generator;

      m_index++;
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

std::string CVisualizer::JSON(const std::string& postjson, const std::string& source)
{
  int64_t index = -1;
  int64_t timeout = 0;

  string* error;
  CJSONElement* json = ParseJSON(postjson, error);

  if (error)
  {
    LogError("%s: %s", source.c_str(), error->c_str());
    delete error;
  }
  else if (!json->IsMap())
  {
    LogError("%s: invalid value for root element: %s", source.c_str(), ToJSON(json).c_str());
  }
  else
  {
    JSONMap::iterator jsonindex = json->AsMap().find("index");
    if (jsonindex != json->AsMap().end() && !jsonindex->second->IsNumber())
      LogError("%s: invalid value for index: %s", source.c_str(), ToJSON(jsonindex->second).c_str());
    else
      index = jsonindex->second->ToInt64();

    JSONMap::iterator jsontimeout = json->AsMap().find("timeout");
    if (jsontimeout != json->AsMap().end() && !jsontimeout->second->IsNumber())
      LogError("%s: invalid value for timeout: %s", source.c_str(), ToJSON(jsontimeout->second).c_str());
    else
      timeout = jsontimeout->second->ToInt64();
  }

  delete json;

  CLock lock(m_viscond);

  //wait for the port index to change with the client requested timeout
  //the maximum timeout is one minute
  if (index == (int64_t)m_index && timeout > 0 && !m_stop)
    m_viscond.Wait(Min(timeout, 60000) * 1000, m_index, (unsigned int)index);

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

  //jack thread name needs to be set in the jack callback
  m_nameset = false;

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

CJSONGenerator* CVisualizer::VisualizersToJSON(bool values)
{
  CJSONGenerator* generator = new CJSONGenerator(true);

  generator->MapOpen();

  generator->AddString("interval");
  generator->AddInt(m_interval);

  generator->AddString("visualizers");
  generator->ArrayOpen();
  for (vector<CVisType>::iterator it = m_visualizers.begin(); it != m_visualizers.end(); it++)
  {
    it->ToJSON(*generator, values);
    it->ClearVisOutput();
  }
  generator->ArrayClose();

  if (values)
  {
    generator->AddString("index");
    generator->AddInt(m_index + 1);
  }

  generator->MapClose();

  return generator;
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

  //set the name of this thread if needed
  if (!m_nameset)
  {
    CThread::SetCurrentThreadName("jack visualizer");
    m_nameset = true;
  }
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

