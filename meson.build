project('ModernCore', 'c', 'cpp', default_options: ['cpp_std=c++20'])

files = [
    'src/Allocator.cpp',
    'src/Backend.cpp',
    'src/Compositor.cpp',
    'src/DataDeviceManager.cpp',
    'src/Display.cpp',
    'src/Logs.cpp',
    'src/MCore.cpp',
    'src/Output.cpp',
    'src/Renderer.cpp',
    'src/Scene.cpp',
    'src/Server.cpp',
    'src/Subcompositor.cpp',
]

compiler = meson.get_compiler('cpp')

pixman = dependency('pixman-1')
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
    files,
    dependencies: [pixman, vulkan, wayland, wlroots]
)
