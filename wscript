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

  conf.check(header_name='assert.h')
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
  conf.check(header_name='pcrecpp.h')
  conf.check(header_name='poll.h')
  conf.check(header_name='pthread.h')
  conf.check(header_name='signal.h')
  conf.check(header_name='stdio.h')
  conf.check(header_name='stdlib.h')
  conf.check(header_name='string.h')
  conf.check(header_name='sys/mman.h')
  conf.check(header_name='sys/signalfd.h')
  conf.check(header_name='sys/stat.h')
  conf.check(header_name='sys/time.h')
  conf.check(header_name='time.h')
  conf.check(header_name='tinyxml.h')
  conf.check(header_name='unistd.h')

#microhttpd.h needs these
  conf.check(header_name='stdarg.h', auto_add_header_name=True)
  conf.check(header_name='stdint.h', auto_add_header_name=True)
  conf.check(header_name='sys/socket.h', auto_add_header_name=True)

  conf.check(header_name='microhttpd.h')

  conf.check(lib='pthread', uselib_store='pthread', mandatory=False)
  conf.check(lib='m', uselib_store='m', mandatory=False)
  conf.check(lib='dl', uselib_store='dl', mandatory=False)
  conf.check(lib='tinyxml', uselib_store='tinyxml')
  conf.check(lib='jack', uselib_store='jack')
  conf.check(lib='pcrecpp', uselib_store='pcrecpp')
  conf.check(lib='microhttpd', uselib_store='microhttpd')

  conf.check(function_name='clock_gettime', header_name='time.h', mandatory=False)
  conf.check(function_name='clock_gettime', header_name='time.h', lib='rt', uselib_store='rt', mandatory=False,
             msg='Checking for clock_gettime in librt')

  conf.write_config_header('config.h')

def build(bld):
  bld.program(source='src/main.cpp\
                      src/bobdsp.cpp\
                      src/httpserver.cpp\
                      src/jackclient.cpp\
                      src/ladspainstance.cpp\
                      src/ladspaplugin.cpp\
                      src/portconnector.cpp\
                      src/util/condition.cpp\
                      src/util/log.cpp\
                      src/util/misc.cpp\
                      src/util/mutex.cpp\
                      src/util/timeutils.cpp',
              use=['m','pthread','rt','dl','tinyxml','jack', 'pcrecpp', 'microhttpd'],        
              includes='./src',
              cxxflags='-Wall -g',
              target='bobdsp')
