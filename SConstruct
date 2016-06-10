# Top-level build wrapper so build outputs go in a separate directory.
import multiprocessing
import os

SetOption('num_jobs', multiprocessing.cpu_count() + 1)

env = Environment(ENV=os.environ, tools=['mingw'])  # this forces linux-style parameters, which gcc-arm expects
Export('env')

###
### Imports
###
SConscript('mbed-scons/SConscript-mbed', duplicate=0)

###
### GCC-ARM environment variables
###
env['AR'] = 'arm-none-eabi-ar'
env['AS'] = 'arm-none-eabi-as'
env['CC'] = 'arm-none-eabi-gcc'
env['CXX'] = 'arm-none-eabi-g++'
env['LINK'] = 'arm-none-eabi-g++'                # predefined is 'arm-none-eabi-gcc'
env['RANLIB'] = 'arm-none-eabi-ranlib'
env['OBJCOPY'] = 'arm-none-eabi-objcopy'
env['OBJDUMP'] = 'arm-none-eabi-objdump'
env['PROGSUFFIX'] = '.elf'

###
### Platform-specific build targets for mbed libraries
###
env.Append(CCFLAGS = '-Os')

SConscript('mbed-scons/targets/SConscript-mbed-env-stm32f303k8', exports='env',
           duplicate=0)

# env.Append(CXXFLAGS = '-std=c++11')  # Live on the bleeding edge, override the mbed default

###
### Actual build targets here
###
SConscript('SConscript-bootloader', variant_dir='build/bootloader',
           duplicate=0)
SConscript('SConscript-application', variant_dir='build/application',
           duplicate=0)

# From https://stackoverflow.com/questions/1094841/reusable-library-to-get-human-readable-version-of-file-size
def sizeof_fmt(num, suffix='B'):
    for unit in ['','Ki','Mi','Gi','Ti','Pi','Ei','Zi']:
        if abs(num) < 1024.0:
            return "%3.3f%s%s" % (num, unit, suffix)
        num /= 1024.0
    return "%.1f%s%s" % (num, 'Yi', suffix)

def display_file_size(target, source, env):
  for source in source:
    name = source.path
    if os.path.exists(source.abspath):
      size = sizeof_fmt(os.path.getsize(source.abspath))
    else:
      size = "MISSING"
    print("%s: %s" % (name, size))

bootloader_size = Command('bootloader_size',
                          [File('build/bootloader.bin'), File('build/application.bin')],
                          display_file_size)
Depends(bootloader_size, DEFAULT_TARGETS)
