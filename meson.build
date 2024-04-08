project('ModernCore', 'c', 'cpp', default_options: ['cpp_std=c++20'])

# common setup

compiler = meson.get_compiler('cpp')
compile_args = compiler.get_supported_arguments(['-march=native'])
add_project_arguments(compile_args, language: ['c', 'cpp'])

if get_option('buildtype').startswith('debug')
    add_project_arguments('-DDEBUG', language: ['c', 'cpp'])
else
    add_project_arguments('-DNDEBUG', language: ['c', 'cpp'])
endif

if get_option('tracy_enable')
    if get_option('buildtype') != 'debugoptimized'
        warning('Profiling builds should set --buildtype=debugoptimized')
    endif
    if get_option('default_library') != 'static'
        error('Profiling builds should set --default-library=static')
    endif
    frame_pointer_args = compiler.get_supported_arguments([
        '-fno-omit-frame-pointer'
    ])
    add_project_arguments(frame_pointer_args, language: ['c', 'cpp'])
endif

# common dependencies

vulkan = dependency('vulkan')
wayland_client = dependency('wayland-client')
tracy = dependency('tracy', static: true)

incdir = include_directories('src')

# wayland protocols

wl_proto = dependency('wayland-protocols')
wl_scanner = dependency('wayland-scanner')

wl_proto_dir = wl_proto.get_variable('pkgdatadir')
wl_scanner_bin = find_program(wl_scanner.get_variable('wayland_scanner'))

protocols = {
    'xdg-shell': wl_proto_dir / 'stable/xdg-shell/xdg-shell.xml',
    'xdg-decoration': wl_proto_dir / 'unstable/xdg-decoration/xdg-decoration-unstable-v1.xml',
    'fractional-scale': wl_proto_dir / 'staging/fractional-scale/fractional-scale-v1.xml',
    'viewporter': wl_proto_dir / 'stable/viewporter/viewporter.xml',
    'cursor-shape': wl_proto_dir / 'staging/cursor-shape/cursor-shape-v1.xml',
    'tablet': wl_proto_dir / 'unstable/tablet/tablet-unstable-v2.xml',
}

wayland_src = []
wayland_server_src = []

foreach name, path : protocols
    code = custom_target(
        name.underscorify() + '_c',
        input: path,
        output: '@BASENAME@-protocol.c',
        command: [wl_scanner_bin, 'private-code', '@INPUT@', '@OUTPUT@']
    )
    wayland_src += code

    header = custom_target(
        name.underscorify() + '_h',
        input: path,
        output: '@BASENAME@-protocol.h',
        command: [wl_scanner_bin, 'server-header', '@INPUT@', '@OUTPUT@']
    )
    wayland_server_src += header

    client_header = custom_target(
        name.underscorify() + '_client_h',
        input: path,
        output: '@BASENAME@-client-protocol.h',
        command: [wl_scanner_bin, 'client-header', '@INPUT@', '@OUTPUT@']
    )
    wayland_src += client_header
endforeach

# mcoreutil

mcoreutil_src = [
    'contrib/ini/ini.c',
    'contrib/libbacktrace/alloc.c',
    'contrib/libbacktrace/dwarf.c',
    'contrib/libbacktrace/elf.c',
    'contrib/libbacktrace/fileline.c',
    'contrib/libbacktrace/mmapio.c',
    'contrib/libbacktrace/posix.c',
    'contrib/libbacktrace/sort.c',
    'contrib/libbacktrace/state.c',
    'src/util/ArgParser.cpp',
    'src/util/Bitmap.cpp',
    'src/util/Callstack.cpp',
    'src/util/Config.cpp',
    'src/util/FileBuffer.cpp',
    'src/util/Home.cpp',
    'src/util/Logs.cpp',
    'src/util/TaskDispatch.cpp',
]

mcoreutil = library('mcoreutil',
    mcoreutil_src,
    include_directories: incdir,
    dependencies: [tracy],
    install: true
)

# mcoreimage

mcoreimage_src = [
    'src/image/bcdec.c',
    'src/image/DdsLoader.cpp',
    'src/image/HeifLoader.cpp',
    'src/image/ImageLoader.cpp',
    'src/image/JpgLoader.cpp',
    'src/image/JxlLoader.cpp',
    'src/image/PngLoader.cpp',
    'src/image/PvrLoader.cpp',
    'src/image/WebpLoader.cpp',
]

mcoreimage = library('mcoreimage',
    mcoreimage_src,
    include_directories: incdir,
    link_with: mcoreutil,
    dependencies: [
        dependency('libpng'),
        dependency('libjpeg'),
        dependency('libjxl'), dependency('libjxl_threads'),
        dependency('libwebp'),
        dependency('libheif'),
        tracy
    ],
    install: true
)

# mcorevulkan

mcorevulkan_src = [
    'src/vulkan/ext/DeviceInfo.cpp',
    'src/vulkan/ext/PhysDevSel.cpp',
    'src/vulkan/VlkAllocator.cpp',
    'src/vulkan/VlkBuffer.cpp',
    'src/vulkan/VlkCommandBuffer.cpp',
    'src/vulkan/VlkCommandPool.cpp',
    'src/vulkan/VlkDescriptorSetLayout.cpp',
    'src/vulkan/VlkDevice.cpp',
    'src/vulkan/VlkFence.cpp',
    'src/vulkan/VlkImage.cpp',
    'src/vulkan/VlkImageView.cpp',
    'src/vulkan/VlkInstance.cpp',
    'src/vulkan/VlkPhysicalDevice.cpp',
    'src/vulkan/VlkPipelineLayout.cpp',
    'src/vulkan/VlkPipeline.cpp',
    'src/vulkan/VlkProxy.cpp',
    'src/vulkan/VlkSampler.cpp',
    'src/vulkan/VlkSemaphore.cpp',
    'src/vulkan/VlkShader.cpp',
    'src/vulkan/VlkShaderModule.cpp',
    'src/vulkan/VlkSwapchain.cpp',
    'src/vulkan/VlkSwapchainProperties.cpp',
]

mcorevulkan = library('mcorevulkan',
    mcorevulkan_src,
    include_directories: incdir,
    link_with: mcoreutil,
    dependencies: [vulkan, tracy],
    install: true
)

# mcorewayland

mcorewayland_src = [
    'src/wayland/WaylandDisplay.cpp',
    'src/wayland/WaylandKeyboard.cpp',
    'src/wayland/WaylandPointer.cpp',
    'src/wayland/WaylandSeat.cpp',
    'src/wayland/WaylandWindow.cpp',
]

mcorewayland = library('mcorewayland',
    mcorewayland_src,
    wayland_src,
    include_directories: incdir,
    link_with: [mcoreutil, mcorevulkan],
    dependencies: [vulkan, wayland_client, tracy],
    install: true
)

# util tests

tests = [
    'util/Config',
    'util/Bitmap',
    'util/FileBuffer',
    'util/FileWrapper',
    'util/Home',
    'util/Logs',
    'util/Panic',
]

gtest = dependency('gtest')

foreach test : tests
    testexe = executable('test_' + test.underscorify(),
        'tests/TestEntry.cpp',
        'tests/' + test + 'Test.cpp',
        link_with: mcoreutil,
        dependencies: gtest,
        include_directories: incdir
    )
    test( test, testexe )
endforeach

# mcore

mcore_src = [
    'src/cursor/CursorBase.cpp',
    'src/cursor/CursorBaseMulti.cpp',
    'src/cursor/CursorLogic.cpp',
    'src/cursor/CursorTheme.cpp',
    'src/cursor/WinCursor.cpp',
    'src/cursor/XCursor.cpp',
    'src/dbus/DbusMessage.cpp',
    'src/dbus/DbusSession.cpp',
    'src/backend/drm/BackendDrm.cpp',
    'src/backend/drm/DrmBuffer.cpp',
    'src/backend/drm/DrmConnector.cpp',
    'src/backend/drm/DrmCrtc.cpp',
    'src/backend/drm/DrmDevice.cpp',
    'src/backend/drm/DrmPlane.cpp',
    'src/backend/drm/DrmProperties.cpp',
    'src/backend/drm/PciBus.cpp',
    'src/backend/wayland/BackendWayland.cpp',
    'src/backend/wayland/WaylandConnector.cpp',
    'src/backend/wayland/WaylandOutput.cpp',
    'src/plumbing/Display.cpp',
    'src/render/Background.cpp',
    'src/render/SoftwareCursor.cpp',
    'src/render/Texture.cpp',
    'src/server/GpuDevice.cpp',
    'src/server/Server.cpp',
    'src/MCore.cpp',
]

shaders = [
    'shader/Background.frag',
    'shader/Background.vert',
    'shader/SoftwareCursor.frag',
    'shader/SoftwareCursor.vert',
]

glslc = find_program('glslc')

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
        install_dir: get_option('datadir') + '/mcore/shaders'
    )
    mcore_src += spirv
endforeach

deps = [
    vulkan,
    wayland_client,
    tracy,
    dependency('wayland-server'),
    dependency('xkbcommon'),
    dependency('libsystemd'),
    dependency('libdrm'),
    dependency('libdisplay-info'),
    dependency('gbm'),
]

executable('mcore',
    mcore_src,
    wayland_src,
    link_with: [mcoreutil, mcoreimage, mcorevulkan, mcorewayland],
    dependencies: deps,
    include_directories: incdir,
    install: true
)

# afiv

afiv_src = [
    'src/afiv/afiv.cpp',
    'src/render/Texture.cpp',
]

executable('afiv',
    afiv_src,
    link_with: [mcoreutil, mcoreimage, mcorevulkan, mcorewayland],
    include_directories: incdir,
    dependencies: [tracy, vulkan],
    install: true
)
