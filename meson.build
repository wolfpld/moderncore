project('ModernCore', 'c', 'cpp', default_options: ['cpp_std=c++20'])

compiler = meson.get_compiler('cpp')

vulkan = dependency('vulkan')
wayland = dependency('wayland-server')
wlroots = dependency('wlroots')

if get_option('buildtype').startswith('debug')
    add_project_arguments('-DDEBUG', language: 'cpp')
endif

compile_args = compiler.get_supported_arguments([
    '-march=native'
])
add_project_arguments(compile_args, language: ['c', 'cpp'])

executable('mcore',
    'src/Display.cpp',
    'src/Logs.cpp',
    'src/MCore.cpp',
    'src/Server.cpp',
    dependencies: [vulkan, wayland, wlroots]
)
