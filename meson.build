project('ModernCore', 'c', 'cpp', default_options: ['cpp_std=c++20'])

entry_point = 'src/MCore.cpp'

files = [
    'contrib/ini/ini.c',
    'contrib/libbacktrace/alloc.c',
    'contrib/libbacktrace/dwarf.c',
    'contrib/libbacktrace/elf.c',
    'contrib/libbacktrace/fileline.c',
    'contrib/libbacktrace/macho.c',
    'contrib/libbacktrace/mmapio.c',
    'contrib/libbacktrace/posix.c',
    'contrib/libbacktrace/sort.c',
    'contrib/libbacktrace/state.c',
    'src/cursor/CursorBase.cpp',
    'src/cursor/CursorBaseMulti.cpp',
    'src/cursor/CursorLogic.cpp',
    'src/cursor/CursorTheme.cpp',
    'src/cursor/WinCursor.cpp',
    'src/cursor/XCursor.cpp',
    'src/dbus/DbusMessage.cpp',
    'src/dbus/DbusSession.cpp',
    'src/image/HeifLoader.cpp',
    'src/image/ImageLoader.cpp',
    'src/image/JpgLoader.cpp',
    'src/image/JxlLoader.cpp',
    'src/image/PngLoader.cpp',
    'src/image/WebpLoader.cpp',
    'src/backend/drm/BackendDrm.cpp',
    'src/backend/drm/DrmConnector.cpp',
    'src/backend/drm/DrmCrtc.cpp',
    'src/backend/drm/DrmDevice.cpp',
    'src/backend/wayland/BackendWayland.cpp',
    'src/backend/wayland/WaylandConnector.cpp',
    'src/backend/wayland/WaylandKeyboard.cpp',
    'src/backend/wayland/WaylandOutput.cpp',
    'src/backend/wayland/WaylandPointer.cpp',
    'src/backend/wayland/WaylandSeat.cpp',
    'src/backend/wayland/WaylandWindow.cpp',
    'src/plumbing/Display.cpp',
    'src/render/Background.cpp',
    'src/render/SoftwareCursor.cpp',
    'src/render/Texture.cpp',
    'src/server/GpuConnectors.cpp',
    'src/server/GpuDevices.cpp',
    'src/server/Server.cpp',
    'src/util/Bitmap.cpp',
    'src/util/Callstack.cpp',
    'src/util/Config.cpp',
    'src/util/FileBuffer.cpp',
    'src/util/Home.cpp',
    'src/util/Logs.cpp',
    'src/vulkan/PhysDevSel.cpp',
    'src/vulkan/VlkAllocator.cpp',
    'src/vulkan/VlkBuffer.cpp',
    'src/vulkan/VlkCommandBuffer.cpp',
    'src/vulkan/VlkCommandPool.cpp',
    'src/vulkan/VlkDescriptorSetLayout.cpp',
    'src/vulkan/VlkDevice.cpp',
    'src/vulkan/VlkFence.cpp',
    'src/vulkan/VlkFramebuffer.cpp',
    'src/vulkan/VlkImage.cpp',
    'src/vulkan/VlkImageView.cpp',
    'src/vulkan/VlkInstance.cpp',
    'src/vulkan/VlkPhysicalDevice.cpp',
    'src/vulkan/VlkPipelineLayout.cpp',
    'src/vulkan/VlkPipeline.cpp',
    'src/vulkan/VlkProxy.cpp',
    'src/vulkan/VlkRenderPass.cpp',
    'src/vulkan/VlkSampler.cpp',
    'src/vulkan/VlkSemaphore.cpp',
    'src/vulkan/VlkShader.cpp',
    'src/vulkan/VlkShaderModule.cpp',
    'src/vulkan/VlkSwapchain.cpp',
    'src/vulkan/VlkSwapchainProperties.cpp',
]

shaders = [
    'shader/Background.frag',
    'shader/Background.vert',
    'shader/SoftwareCursor.frag',
    'shader/SoftwareCursor.vert',
]

tests = [
    'util/Config',
    'util/Bitmap',
    'util/FileBuffer',
    'util/FileWrapper',
    'util/Home',
    'util/Logs',
    'util/Panic',
]

compiler = meson.get_compiler('cpp')
glslc = find_program('glslc')

vulkan = dependency('vulkan')
wayland_server = dependency('wayland-server')
wayland_client = dependency('wayland-client')
xkbcommon = dependency('xkbcommon')
wl_proto = dependency('wayland-protocols')
wl_scanner = dependency('wayland-scanner')
glm = dependency('glm')
libpng = dependency('libpng')
libjpeg = dependency('libjpeg')
libjxl = [dependency('libjxl'), dependency('libjxl_threads')]
libwebp = dependency('libwebp')
libheif = dependency('libheif')
libsystemd = dependency('libsystemd')
libdrm = dependency('libdrm')
libdisplayinfo = dependency('libdisplay-info')
libgbm = dependency('gbm')
gtest = dependency('gtest')

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

foreach shader : shaders
    spirv = custom_target(
        shader.underscorify(),
        input: shader,
        output: '@PLAINNAME@.spv',
        command: [
            glslc,
            '-O',
            '-o', '@OUTPUT@',
            '@INPUT@',
        ],
        install: true,
        install_dir: '/'
    )
    files += spirv
endforeach

if get_option('buildtype').startswith('debug')
    add_project_arguments('-DDEBUG', language: 'cpp')
endif

compile_args = compiler.get_supported_arguments([
    '-march=native'
])
add_project_arguments(compile_args, language: ['c', 'cpp'])

deps = [
    vulkan,
    wayland_server,
    wayland_client,
    xkbcommon,
    glm,
    libpng,
    libjpeg,
    libjxl,
    libwebp,
    libheif,
    libsystemd,
    libdrm,
    libdisplayinfo,
    libgbm,
]

mcorelib = static_library('mcorelib',
    files,
    dependencies: deps,
)

executable('mcore',
    entry_point,
    link_with: mcorelib,
    dependencies: deps,
    install: true,
    install_dir: '/'
)

foreach test : tests
    testexe = executable('test_' + test.underscorify(),
        'tests/TestEntry.cpp',
        'tests/' + test + 'Test.cpp',
        link_with: mcorelib,
        dependencies: [deps, gtest],
    )
    test( test, testexe )
endforeach
