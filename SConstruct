# Top-level build wrapper so build outputs go in a separate directory.
import multiprocessing
import os

SetOption('num_jobs', multiprocessing.cpu_count() + 1)

env = Environment(ENV={'PATH' : os.environ['PATH']}, tools=['mingw'])  # this forces linux-style parameters, which gcc-arm expects
Export('env')

##
## Go fast!
##
env.Decider('MD5-timestamp')

##
## Debugging output optimization
##

def simplify_output(env, mappings):
  pad_len = max([len(val) for val in mappings.values()]) + 2
  for key, val in mappings.items():
    env[key] = val + (' ' * (pad_len - len(val))) + '$TARGET'

if ARGUMENTS.get('VERBOSE') != '1':
  simplify_output(env, {
    'ASPPCOMSTR': 'AS',
    'ASCOMSTR': 'AS',
    'ARCOMSTR': 'AR',
    'CCCOMSTR': 'CC',
    'CXXCOMSTR': 'CXX',
    'LINKCOMSTR': 'LD',
    'RANLIBCOMSTR': 'RANLIB',
    'OBJCOPYCOMSTR': 'BIN',
    'OBJDUMPCOMSTR': 'DUMP',
    'SYMBOLSCOMSTR': 'SYM',
    'SYMBOLSIZESCOMSTR': 'SYM',
  })

###
### Imports
###
SConscript('mbed-scons/SConscript-env-platforms', duplicate=0)
SConscript('mbed-scons/SConscript-env-gcc-arm', duplicate=0)
SConscript('mbed-scons/SConscript-mbed', duplicate=0)

###
### Platform-specific build targets for mbed libraries
###
env.Append(CCFLAGS = '-Os')
env['MBED_TARGETS_JSON_FILE'] = File('mbed/hal/targets.json').srcnode()

###
### Actual build targets here
###
builds = [
  SConscript('SConscript-bootloader-stm32f303k8',
      variant_dir='build/bootloader-stm32f303k8',
      duplicate=0),
  SConscript('SConscript-application-stm32f303k8',
      variant_dir='build/application-stm32f303k8',
      duplicate=0),
  SConscript('SConscript-bootloader-stm32l432kc',
      variant_dir='build/bootloader-stm32l432kc',
      duplicate=0),
  SConscript('SConscript-application-stm32l432kc',
      variant_dir='build/application-stm32l432kc',
      duplicate=0),
]

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

bootloader_size = Command('bootloader_size', builds, display_file_size)
Depends(bootloader_size, DEFAULT_TARGETS)
