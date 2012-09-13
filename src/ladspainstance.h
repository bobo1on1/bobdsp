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
#include <vector>
#include <jack/jack.h>

#include "ladspaplugin.h"

typedef std::pair<std::string, float> portvalue;

class CPort
{
  public:
    CPort(jack_port_t* jackport, unsigned long ladspaport, bool isinput);
    ~CPort();

    void          CheckBufferSize(jack_nframes_t nframes, float gain);

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
        CLadspaPlugin* plugin, std::vector<portvalue>& controlinputs, int samplerate);
    ~CLadspaInstance();

    bool Connect();
    void Disconnect(bool unregisterjack = true);
    void Activate();
    void Deactivate();
    void Run(jack_nframes_t nframes, float pregain, float postgain);

  private:
    std::string             m_name;
    std::vector<portvalue>& m_controlinputs;
    std::vector<portvalue>  m_controloutputs;
    jack_client_t*          m_client;
    int                     m_instance;
    int                     m_totalinstances;
    CLadspaPlugin*          m_plugin;
    int                     m_samplerate;
    LADSPA_Descriptor*      m_descriptor;
    LADSPA_Handle           m_handle;
    std::vector<CPort>      m_ports;
    bool                    m_activated;
};

#endif //LADSPAINSTANCE_H
