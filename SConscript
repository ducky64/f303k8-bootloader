import os

Import('env')

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

def build_target(env, mbed_target, name, sources_dir,
    mbed_additional=[], mbed_ignores=[], 
    linkflags=[], linkscript=None):
  env = env.Clone()
  
  env.ConfigureMbedTarget(mbed_target, File('mbed/hal/targets.json').srcnode())
  env.VariantDir(name, '.')
  
  mbed_paths = env.GetMbedSourceDirectories(Dir(name).Dir('mbed/hal'))
  env.Append(CPPPATH=[x.srcnode().srcnode() for x in mbed_paths])  # this allows duplicate=0
  if linkscript is None:
    env['MBED_LINKSCRIPT'] = env.GetMbedLinkscript(mbed_paths)
  else:
    env['MBED_LINKSCRIPT'] = linkscript

  env_mbed = env.Clone()
  env_mbed.Append(CCFLAGS = ['-w'])  # don't care about errors in dependencies
  mbed_sources = env.GetMbedSources(mbed_paths)
  mbed_sources.extend([Dir(name).File(x) for x in mbed_additional])
  for mbed_ignore in mbed_ignores:
    mbed_sources.remove(Dir(name).File(mbed_ignore))
    
  mbed_lib = env_mbed.StaticLibrary(os.path.join(name, 'mbed'), mbed_sources)
  
  env.Prepend(LIBS = mbed_lib)
  env.Append(LINKFLAGS=[
    '-Wl,--whole-archive',  # used to compile mbed HAL, which uses funky weak symbols
    mbed_lib,
    '-Wl,--no-whole-archive',
    '--specs=nosys.specs',
  ])

  env.Append(CCFLAGS = ['-Werror', '-Wall'])  # strict warnings in user program


  program = env.Program(name,
      Dir(name).Dir(sources_dir).glob('*.cpp') + Dir(name).Dir(sources_dir).glob('*.S'),
      LINKFLAGS=env['LINKFLAGS'] + linkflags 
  )
  env.Depends(program, env['MBED_LINKSCRIPT'])
  binary = env.Binary(program)
  env.Objdump(program)
  env.SymbolsSize(program)

  return binary

env.Append(CCFLAGS='-Os')

builds = [
  build_target(env, 'NUCLEO_L432KC', 'application-nucleo-l432kc', 'application'),
  build_target(env, 'NUCLEO_L432KC', 'bootloader-nucleo-l432kc', 'bootloader',
    mbed_additional=[
      'mbed-overrides/stm32l432kc-bootloader/cmsis_nvic.c',
      'mbed-overrides/stm32l432kc-bootloader/i2c_api.c',
      'mbed-overrides/stm32l432kc-bootloader/boot_vector.S',
    ],
    mbed_ignores=[
      'mbed/hal/targets/cmsis/TARGET_STM/TARGET_STM32L4/TARGET_NUCLEO_L432KC/cmsis_nvic.c',
      'mbed/hal/targets/hal/TARGET_STM/TARGET_STM32L4/i2c_api.c',
    ],
    linkscript='mbed-overrides/stm32l432kc-bootloader/STM32L432XX.ld',
    linkflags='-Wl,--wrap=error',
  ),
  build_target(env, 'NUCLEO_F303K8', 'application-nucleo-f303k8', 'application'),
  build_target(env, 'NUCLEO_F303K8', 'bootloader-nucleo-f303k8', 'bootloader',
    mbed_additional=[
      'mbed-overrides/stm32f303k8-bootloader/cmsis_nvic.c',
      'mbed-overrides/stm32f303k8-bootloader/i2c_api.c',
      'mbed-overrides/stm32f303k8-bootloader/boot_vector.S',
    ],
    mbed_ignores=[
      'mbed/hal/targets/cmsis/TARGET_STM/TARGET_STM32F3/TARGET_NUCLEO_F303K8/cmsis_nvic.c',
      'mbed/hal/targets/hal/TARGET_STM/TARGET_STM32F3/i2c_api.c',
    ],
    linkscript='mbed-overrides/stm32f303k8-bootloader/STM32F303X8.ld',
    linkflags='-Wl,--wrap=error',
  )
]

bootloader_size = Command('bootloader_size', builds, display_file_size)
Depends(bootloader_size, DEFAULT_TARGETS)
