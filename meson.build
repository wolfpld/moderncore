project('ModernCore', 'c', 'cpp', default_options: ['cpp_std=c++20'])

files = [
    'contrib/ini/ini.c',
    'src/MCore.cpp',
    'src/Server.cpp',
    'src/cursor/CursorBase.cpp',
    'src/cursor/CursorBaseMulti.cpp',
    'src/cursor/CursorTheme.cpp',
    'src/cursor/WinCursor.cpp',
    'src/cursor/XCursor.cpp',
    'src/backend/wayland/BackendWayland.cpp',
    'src/backend/wayland/WaylandKeyboard.cpp',
    'src/backend/wayland/WaylandOutput.cpp',
    'src/backend/wayland/WaylandPointer.cpp',
    'src/backend/wayland/WaylandSeat.cpp',
    'src/backend/wayland/WaylandWindow.cpp',
    'src/plumbing/Display.cpp',
    'src/util/Bitmap.cpp',
    'src/util/Config.cpp',
    'src/util/Home.cpp',
    'src/util/Logs.cpp',
    'src/vulkan/PhysDevSel.cpp',
    'src/vulkan/VlkAllocator.cpp',
    'src/vulkan/VlkBuffer.cpp',
    'src/vulkan/VlkCommandBuffer.cpp',
    'src/vulkan/VlkCommandPool.cpp',
    'src/vulkan/VlkDevice.cpp',
    'src/vulkan/VlkFence.cpp',
    'src/vulkan/VlkFramebuffer.cpp',
    'src/vulkan/VlkInfo.cpp',
    'src/vulkan/VlkInstance.cpp',
    'src/vulkan/VlkPhysicalDevice.cpp',
    'src/vulkan/VlkProxy.cpp',
    'src/vulkan/VlkRenderPass.cpp',
    'src/vulkan/VlkSemaphore.cpp',
    'src/vulkan/VlkShaderModule.cpp',
    'src/vulkan/VlkSwapchain.cpp',
    'src/vulkan/VlkSwapchainProperties.cpp',
]

compiler = meson.get_compiler('cpp')

vulkan = dependency('vulkan')
wayland_server = dependency('wayland-server')
wayland_client = dependency('wayland-client')
xkbcommon = dependency('xkbcommon')
wl_proto = dependency('wayland-protocols')
wl_scanner = dependency('wayland-scanner')

wl_proto_dir = wl_proto.get_variable('pkgdatadir')
wl_scanner_bin = find_program(wl_scanner.get_variable('wayland_scanner'))

protocols = {
    'xdg-shell': wl_proto_dir / 'stable/xdg-shell/xdg-shell.xml',
    'xdg-decoration': wl_proto_dir / 'unstable/xdg-decoration/xdg-decoration-unstable-v1.xml',
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

    client_header = custom_target(
        name.underscorify() + '_client_h',
        input: path,
        output: '@BASENAME@-client-protocol.h',
        command: [wl_scanner_bin, 'client-header', '@INPUT@', '@OUTPUT@']
    )
    files += client_header
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
    dependencies: [vulkan, wayland_server, wayland_client, xkbcommon]
)
