project('ModernCore', 'c', 'cpp', default_options: ['cpp_std=c++20'])

compiler = meson.get_compiler('cpp')

vulkan = dependency('vulkan')

if get_option('buildtype').startswith('debug')
    add_project_arguments('-DDEBUG', language: 'cpp')
endif

compile_args = compiler.get_supported_arguments([
    '-march=native'
])
add_project_arguments(compile_args, language: ['c', 'cpp'])

executable('mcore',
    'src/MCore.cpp',
    dependencies: [vulkan]
)
