#! /usr/bin/env python
# encoding: utf-8

# the following two variables are used by the target "waf dist"
VERSION='0.0.1'
APPNAME='bobdsp'

# these variables are mandatory ('/' are converted automatically)
top = '.'
out = 'build'

def options(opt):
  opt.load('compiler_cxx')

def configure(conf):
  conf.load('compiler_cxx')

  conf.check(header_name='arpa/inet.h')
  conf.check(header_name='assert.h')
  conf.check(header_name='ctype.h')
  conf.check(header_name='dirent.h')
  conf.check(header_name='dlfcn.h')
  conf.check(header_name='errno.h')
  conf.check(header_name='fcntl.h')
  conf.check(header_name='inttypes.h')
  conf.check(header_name='jack/jack.h')
  conf.check(header_name='ladspa.h')
  conf.check(header_name='locale.h')
  conf.check(header_name='malloc.h')
  conf.check(header_name='math.h')
  conf.check(header_name='netdb.h')
  conf.check(header_name='netinet/in.h')
  conf.check(header_name='pcrecpp.h')
  conf.check(header_name='poll.h')
  conf.check(header_name='pthread.h')
  conf.check(header_name='samplerate.h')
  conf.check(header_name='signal.h')
  conf.check(header_name='stddef.h')
  conf.check(header_name='stdio.h')
  conf.check(header_name='stdlib.h')
  conf.check(header_name='string.h')
  conf.check(header_name='sys/mman.h')
  conf.check(header_name='sys/signalfd.h')
  conf.check(header_name='sys/stat.h')
  conf.check(header_name='sys/time.h')
  conf.check(header_name='time.h')
  conf.check(header_name='unistd.h')
  conf.check(header_name='uriparser/Uri.h')
  conf.check(header_name='yajl/yajl_gen.h')
  conf.check(header_name='yajl/yajl_parse.h')
  conf.check(header_name='yajl/yajl_version.h', mandatory=False)

#microhttpd.h needs these
  conf.check(header_name='stdarg.h', auto_add_header_name=True)
  conf.check(header_name='stdint.h', auto_add_header_name=True)
  conf.check(header_name='sys/socket.h', auto_add_header_name=True)

  conf.check(header_name='microhttpd.h')

  conf.check(lib='dl', uselib_store='dl', mandatory=False)
  conf.check(lib='jack', uselib_store='jack')
  conf.check(lib='m', uselib_store='m', mandatory=False)
  conf.check(lib='microhttpd', uselib_store='microhttpd')
  conf.check(lib='pcrecpp', uselib_store='pcrecpp')
  conf.check(lib='pthread', uselib_store='pthread', mandatory=False)
  conf.check(lib='samplerate', uselib_store='samplerate')
  conf.check(lib='uriparser', uselib_store='uriparser')
  conf.check(lib='yajl', uselib_store='yajl')

  conf.check(function_name='pthread_setname_np', header_name='pthread.h', lib='pthread', mandatory=False)
  conf.check(function_name='clock_gettime', header_name='time.h', mandatory=False)
  conf.check(function_name='clock_gettime', header_name='time.h', lib='rt', uselib_store='rt', mandatory=False,
             msg='Checking for clock_gettime in librt')

  conf.write_config_header('config.h')

def build(bld):
  bld.program(source='src/main.cpp\
                      src/bobdsp.cpp\
                      src/clientmessage.cpp\
                      src/clientsmanager.cpp\
                      src/httpserver.cpp\
                      src/jackclient.cpp\
                      src/jsonsettings.cpp\
                      src/ladspainstance.cpp\
                      src/ladspaplugin.cpp\
                      src/pluginmanager.cpp\
                      src/portconnector.cpp\
                      src/visualizer.cpp\
                      src/util/alphanum.cpp\
                      src/util/condition.cpp\
                      src/util/floatbufferops.cpp\
                      src/util/JSON.cpp\
                      src/util/log.cpp\
                      src/util/misc.cpp\
                      src/util/mutex.cpp\
                      src/util/timeutils.cpp\
                      src/util/thread.cpp',
              use=['m','pthread','rt','dl','jack', 'pcrecpp', 'microhttpd', 'samplerate', 'uriparser', 'yajl'],        
              includes='./src',
              cxxflags='-Wall -g -DUTILNAMESPACE=BobDSPUtil',
              target='bobdsp')

#set cssshlib_PATTERN to produce bobdsp.so from target='bobdsp'
  bld.env.cxxshlib_PATTERN = '%s.so'
  bld.shlib(source='src/ladspa/biquad.cpp\
                    src/ladspa/biquadcoefs.cpp\
                    src/ladspa/filterdescriptions.cpp\
                    src/ladspa/filterinterface.cpp\
                    src/ladspa/dither.cpp',
            use=['m'],
            includes='./src',
            cxxflags='-Wall -g -DUTILNAMESPACE=BobDSPLadspa',
            target='bobdsp',
            install_path='${PREFIX}/lib/ladspa')
