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

#ifndef LADSPAINSTANCE_H
#define LADSPAINSTANCE_H

#include <utility>
#include <string>
#include <map>
#include <vector>
#include <jack/jack.h>

#include "ladspaplugin.h"

class controlvalue
{
  public:
    controlvalue()
    {
      m_doubleval = 0.0;
      m_floatval  = 0.0f;
      m_floatorig = 0.0f;
    }


    controlvalue& operator= (double value)
    {
      m_doubleval = value;
      m_floatval  = value;
      m_floatorig = value;

      return *this;
    }

    operator double()
    {
      //m_floatval can be changed via a float*,
      //the original value is stored in m_floatorig
      //if it changed, assign the same value to m_doubleval
      if (m_floatval != m_floatorig)
      {
        m_floatorig = m_floatval;
        m_doubleval = m_floatval;
      }

      return m_doubleval;
    }

    operator float*()
    {
      return &m_floatval;
    }

  private:
    //control values are stored here as double for generating JSON
    //and as float for passing into the LADSPA plugins
    double m_doubleval;
    float  m_floatval;
    float  m_floatorig;

    //don't allow assigning floats
    controlvalue& operator= (float value)
    {
      return *this;
    }

};

typedef std::map<std::string, controlvalue> controlmap;

class CPort
{
  public:
    CPort(jack_port_t* jackport, unsigned long ladspaport, bool isinput);
    ~CPort();

    void          CheckBufferSize(jack_nframes_t nframes, float gain);
    float*        GetBuffer(float* jackptr);
    jack_port_t*  GetJackPort()   { return m_jackport;   }
    bool          IsInput()       { return m_isinput;    }
    unsigned long GetLadspaPort() { return m_ladspaport; }

  private:
    jack_port_t*  m_jackport;
    unsigned long m_ladspaport;
    float*        m_buf;
    unsigned int  m_bufsize;
    bool          m_isinput;
};

class CLadspaInstance
{
  public:
    CLadspaInstance(jack_client_t* client, const std::string& name, int instance, int totalinstances,
        CLadspaPlugin* plugin, controlmap& controlinputs, int samplerate);
    ~CLadspaInstance();

    bool Connect();
    void Disconnect(bool unregisterjack = true);
    void Activate();
    void Deactivate();
    void Run(jack_nframes_t nframes, float pregain, float postgain);

  private:
    std::string        m_name;
    controlmap&        m_controlinputs;
    controlmap         m_controloutputs;
    jack_client_t*     m_client;
    int                m_instance;
    int                m_totalinstances;
    CLadspaPlugin*     m_plugin;
    int                m_samplerate;
    LADSPA_Handle      m_handle;
    std::vector<CPort> m_ports;
    bool               m_activated;
};

#endif //LADSPAINSTANCE_H
