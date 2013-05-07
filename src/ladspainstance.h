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
      m_doubleval   = 0.0;
      m_floatval    = 0.0f;
      m_floatorig   = 0.0f;
      m_floattarget = 0.0f;
      m_floatout    = 0.0f;
      m_smooth      = false;
    }

    controlvalue& operator= (double value)
    {
      m_doubleval   = value;
      m_floattarget = value;
      m_floatorig   = m_floatval;

      return *this;
    }

    //when assigning, only copy the double
    controlvalue& operator= (controlvalue& right)
    {
      *this = right.m_doubleval;
      return *this;
    }

    operator double()
    {
      return m_doubleval;
    }

    operator float*()
    {
      return &m_floatout;
    }

    float FloatVal()
    {
      return m_floatval;
    }

    void SetSmooth(bool smooth)
    {
      m_smooth = smooth;
    }

    void Update(float mul = 1.0f)
    {
      //if the smoother is enabled, move m_floatval towards m_floattarget
      //if m_floatval goes beyond m_floattarget, or if the addition did not change m_floatval
      //assign m_floattarget to m_floatval
      if (m_smooth && mul != 1.0f && m_floatval != m_floattarget)
      {
        float prev = m_floatval;
        m_floatval += (m_floattarget - m_floatorig) * mul;
        if ((m_floattarget > m_floatorig && m_floatval > m_floattarget) ||
            (m_floattarget < m_floatorig && m_floatval < m_floattarget) ||
            prev == m_floatval)
        {
          m_floatval = m_floattarget;
        }
      }
      else
      {
        m_floatval = m_floattarget;
      }

      //m_floatout can be changed by a ladspa plugin using the pointer
      //so reset it here
      m_floatout = m_floatval;
    }

    bool NeedsSmooth()
    {
      if (m_smooth && m_floatval != m_floattarget)
        return true;
      else
        return false;
    }

  private:
    //control values are stored here as double for generating JSON
    //and as float for passing into the LADSPA plugins
    double m_doubleval;
    float  m_floatval;
    float  m_floatorig;
    float  m_floattarget;
    float  m_floatout;
    bool   m_smooth;

    //don't allow assigning floats
    controlvalue& operator= (float value)
    {
      return *this;
    }

    //casting to float forbidden, either cast to double to get m_doubleval
    //or use FloatVal() to get m_floatval
    operator float()
    {
      return 0.0f;
    }
};

typedef std::map<std::string, controlvalue> controlmap;

class CPort
{
  public:
    CPort(jack_port_t* jackport, unsigned long ladspaport, bool isinput);
    ~CPort();

    void          AllocateBuffer(int buffersize);
    float*        GetBuffer(float* jackptr);
    jack_port_t*  GetJackPort()   { return m_jackport;   }
    bool          IsInput()       { return m_isinput;    }
    unsigned long GetLadspaPort() { return m_ladspaport; }

  private:
    jack_port_t*  m_jackport;
    unsigned long m_ladspaport;
    float*        m_buf;
    bool          m_isinput;
};

class CLadspaInstance
{
  public:
    CLadspaInstance(jack_client_t* client, const std::string& name, int instance, int totalinstances,
        CLadspaPlugin* plugin, controlmap& controlinputs, int samplerate, int buffersize);
    ~CLadspaInstance();

    bool Connect();
    void Disconnect();
    void Activate();
    void Deactivate();
    void AllocateBuffers(int buffersize);
    void Run(jack_nframes_t jackframes, int frames, int offset, float pregain, float postgain);

  private:
    std::string        m_name;
    controlmap&        m_controlinputs;
    controlmap         m_controloutputs;
    jack_client_t*     m_client;
    int                m_instance;
    int                m_totalinstances;
    CLadspaPlugin*     m_plugin;
    int                m_samplerate;
    int                m_buffersize;
    LADSPA_Handle      m_handle;
    std::vector<CPort> m_ports;
    bool               m_activated;
};

#endif //LADSPAINSTANCE_H
