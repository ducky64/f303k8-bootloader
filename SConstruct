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

###
### Actual build targets here
###
SConscript('SConscript-bootloader', variant_dir='build/bootloader',
           duplicate=0)
SConscript('SConscript-application', variant_dir='build/application',
           duplicate=0)
