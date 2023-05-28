project('ModernCore', 'c', 'cpp', default_options: ['cpp_std=c++20'])

files = [
    'contrib/ini/ini.c',
    'src/MCore.cpp',
    'src/Server.cpp',
    'src/cursor/CursorBase.cpp',
    'src/cursor/CursorTheme.cpp',
    'src/plumbing/Allocator.cpp',
    'src/plumbing/Backend.cpp',
    'src/plumbing/Compositor.cpp',
    'src/plumbing/Cursor.cpp',
    'src/plumbing/DataDeviceManager.cpp',
    'src/plumbing/Display.cpp',
    'src/plumbing/Keyboard.cpp',
    'src/plumbing/Logs.cpp',
    'src/plumbing/Output.cpp',
    'src/plumbing/OutputNode.cpp',
    'src/plumbing/Pointer.cpp',
    'src/plumbing/Renderer.cpp',
    'src/plumbing/Scene.cpp',
    'src/plumbing/Seat.cpp',
    'src/plumbing/Subcompositor.cpp',
    'src/plumbing/XdgShell.cpp',
    'src/util/Bitmap.cpp',
]

compiler = meson.get_compiler('cpp')

pixman = dependency('pixman-1')
vulkan = dependency('vulkan')
wayland = dependency('wayland-server')
wlroots = dependency('wlroots')
xkbcommon = dependency('xkbcommon')
wl_proto = dependency('wayland-protocols')
wl_scanner = dependency('wayland-scanner')

wl_proto_dir = wl_proto.get_variable('pkgdatadir')
wl_scanner_bin = find_program(wl_scanner.get_variable('wayland_scanner'))

protocols = {
    'xdg-shell': wl_proto_dir / 'stable/xdg-shell/xdg-shell.xml',
}

foreach name, path : protocols
    code = custom_target(
        name.underscorify() + '_c',
        input: path,
        output: '@BASENAME@-protocol.c',
        command: [wl_scanner_bin, 'private-code', '@INPUT@', '@OUTPUT@']
    )
    files += code

    header = custom_target(
        name.underscorify() + '_h',
        input: path,
        output: '@BASENAME@-protocol.h',
        command: [wl_scanner_bin, 'server-header', '@INPUT@', '@OUTPUT@']
    )
    files += header
endforeach


if get_option('buildtype').startswith('debug')
    add_project_arguments('-DDEBUG', language: 'cpp')
endif

compile_args = compiler.get_supported_arguments([
    '-march=native'
])
add_project_arguments(compile_args, language: ['c', 'cpp'])

executable('mcore',
    files,
    dependencies: [pixman, vulkan, wayland, wlroots, xkbcommon]
)
