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

#ifndef VISUALIZER_H
#define VISUALIZER_H

#include "util/inclstdint.h"
#include "util/incltinyxml.h"
#include "util/thread.h"
#include "util/condition.h"
#include "util/misc.h"
#include "util/JSON.h"

#include <string>
#include <vector>
#include <jack/jack.h>
#include <samplerate.h>

enum EVISTYPE
{
  SPECTRUM,
  MEAN,
  RMS,
  PEAK,
};

class CVisType
{
  public:

    CVisType(EVISTYPE type, std::string name, int outsamplerate, int bands);
    ~CVisType();

    bool Connect(jack_client_t* client, int samplerate, int64_t interval);
    void Disconnect(jack_client_t* client, bool unregister);
    void ProcessJack(bool locked, jack_nframes_t nframes, int64_t time);

    void Append(int bufindex, float* data, int samples, int64_t time);
    void Resample(int bufindex, float* data, int samples, int64_t time);

    void ProcessVisualizer(int64_t vistime);

    bool HasVisOutput() { return m_hasvisoutput; }
    void ClearVisOutput();

    void ToJSON(CJSONGenerator& generator);

  private:

    void Allocate(int bufsize);

    void ProcessSpectrum(int64_t vistime);
    void ProcessAverage(int64_t vistime, void(*processfunc)(float, float&, int&));

    static void Square(float in, float& avg, int& samples);
    static void Abs(float in, float& avg, int& samples);
    static void Peak(float in, float& highest, int& samples);

    EVISTYPE     m_type;
    const char*  m_typestr;
    std::string  m_name;
    int          m_bands;
    jack_port_t* m_port;
    float*       m_buf[2];
    int          m_bufsize[2];
    int          m_bufpos[2];
    int          m_buffill[2];
    int64_t      m_buftime[2];

    int          m_samplerate;
    int          m_outsamplerate;
    SRC_STATE*   m_srcstate;

    bool         m_hasvisoutput;
    float        m_visamplitude;
    int          m_visamplitudesamples;
};

class CVisualizer : public CThread
{
  public:
    CVisualizer();
    ~CVisualizer();

    void               LoadSettingsFromFile(const std::string& filename);
    void               VisualizersFromXML(TiXmlElement* root);
    jack_status_t      ExitStatus() { return m_exitstatus; }
    const std::string& ExitReason() { return m_exitreason; }

    void               Start();
    void               Process();
    std::string        JSON();
    std::string        JSON(const std::string& postjson);

  private:
    bool                  Connect();
    void                  Disconnect(bool unregisterjack);
    void                  VisualizersToJSON();

    bool                  m_connected;
    bool                  m_wasconnected;
    jack_client_t*        m_client;
    jack_status_t         m_exitstatus;
    std::string           m_exitreason;
    bool                  m_nameset;
    int                   m_samplerate;
    CCondition            m_jackcond;
    int64_t               m_interval;
    int64_t               m_vistime;

    std::vector<CVisType> m_visualizers;

    std::string           m_json;
    unsigned int          m_index;
    CCondition            m_viscond;

    static int  SJackProcessCallback(jack_nframes_t nframes, void *arg);
    void        PJackProcessCallback(jack_nframes_t nframes);

    static void SJackInfoShutdownCallback(jack_status_t code, const char *reason, void *arg);
    void        PJackInfoShutdownCallback(jack_status_t code, const char *reason);
};

#endif //VISUALIZER_H