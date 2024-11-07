cmake_minimum_required(VERSION 3.29)

option(TRACY_ENABLE "Enable Tracy" OFF)
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

set(CMAKE_CXX_STANDARD 20)

project(ModernCore
    LANGUAGES C CXX
)

include(FindPkgConfig)
include(cmake/config.cmake)
include(cmake/CPM.cmake)
include(cmake/FindWaylandScanner.cmake)

if(NOT TRACY_ENABLE)
    set(TRACY_OPTIONS "TRACY_STATIC ON")
endif()

CPMAddPackage(
    NAME tracy
    GITHUB_REPOSITORY wolfpld/tracy
    GIT_TAG master
    OPTIONS ${TRACY_OPTIONS}
    EXCLUDE_FROM_ALL TRUE
)

CPMAddPackage(
    NAME vma
    GITHUB_REPOSITORY GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    GIT_TAG v3.1.0
    EXCLUDE_FROM_ALL TRUE
)

CPMAddPackage(
    NAME stb
    GITHUB_REPOSITORY nothings/stb
    GIT_TAG 2e2bef463a5b53ddf8bb788e25da6b8506314c08
    DOWNLOAD_ONLY TRUE
)

find_program(GLSLC glslc REQUIRED)

pkg_check_modules(DISPLAYINFO REQUIRED libdisplay-info)
pkg_check_modules(DRM REQUIRED libdrm)
pkg_check_modules(GBM REQUIRED gbm)
pkg_check_modules(HEIF REQUIRED libheif)
pkg_check_modules(JPEG REQUIRED libjpeg)
pkg_check_modules(JXL REQUIRED libjxl libjxl_threads)
pkg_check_modules(PNG REQUIRED libpng)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)
pkg_check_modules(VULKAN REQUIRED vulkan)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
pkg_check_modules(WAYLAND_SERVER REQUIRED wayland-server)
pkg_check_modules(WEBP REQUIRED libwebp)

pkg_get_variable(WAYLAND_PROTOCOLS_PKGDATADIR wayland-protocols pkgdatadir)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/src)

if(TRACY_ENABLE)
    add_definitions(-DTRACY_ENABLE)
    add_compile_options(-g -O3 -fno-omit-frame-pointer)
endif()

add_custom_target(git-ref
    COMMAND python3 ${CMAKE_CURRENT_LIST_DIR}/cmake/git-ref.py
    BYPRODUCTS GitRef.hpp
)

# mcoreutil

set(MCOREUTIL_SRC
    contrib/ini/ini.c
    contrib/libbacktrace/alloc.c
    contrib/libbacktrace/dwarf.c
    contrib/libbacktrace/elf.c
    contrib/libbacktrace/fileline.c
    contrib/libbacktrace/mmapio.c
    contrib/libbacktrace/posix.c
    contrib/libbacktrace/sort.c
    contrib/libbacktrace/state.c
    src/util/ArgParser.cpp
    src/util/Bitmap.cpp
    src/util/Callstack.cpp
    src/util/Config.cpp
    src/util/FileBuffer.cpp
    src/util/Home.cpp
    src/util/Logs.cpp
    src/util/TaskDispatch.cpp
    src/util/stb_image_resize_impl.cpp
)

add_library(mcoreutil ${MCOREUTIL_SRC})
target_link_libraries(mcoreutil PRIVATE
    Tracy::TracyClient
)
target_include_directories(mcoreutil PRIVATE ${stb_SOURCE_DIR})

# mcoreimage

set(MCOREIMAGE_SRC
    src/image/bcdec.c
    src/image/DdsLoader.cpp
    src/image/HeifLoader.cpp
    src/image/ImageLoader.cpp
    src/image/JpgLoader.cpp
    src/image/JxlLoader.cpp
    src/image/PngLoader.cpp
    src/image/PvrLoader.cpp
    src/image/WebpLoader.cpp
)

add_library(mcoreimage ${MCOREIMAGE_SRC})
target_link_libraries(mcoreimage PRIVATE
    mcoreutil
    Tracy::TracyClient
    ${HEIF_LIBRARIES}
    ${JPEG_LIBRARIES}
    ${JXL_LIBRARIES}
    ${PNG_LIBRARIES}
    ${WEBP_LIBRARIES}
)
target_include_directories(mcoreimage PRIVATE
    ${HEIF_INCLUDE_DIRS}
    ${JPEG_INCLUDE_DIRS}
    ${JXL_INCLUDE_DIRS}
    ${PNG_INCLUDE_DIRS}
    ${WEBP_INCLUDE_DIRS}
)

# mcorevulkan

set(MCOREVULKAN_SRC
    src/vulkan/ext/DeviceInfo.cpp
    src/vulkan/ext/PhysDevSel.cpp
    src/vulkan/VlkAllocator.cpp
    src/vulkan/VlkBuffer.cpp
    src/vulkan/VlkCommandBuffer.cpp
    src/vulkan/VlkCommandPool.cpp
    src/vulkan/VlkDescriptorSetLayout.cpp
    src/vulkan/VlkDevice.cpp
    src/vulkan/VlkFence.cpp
    src/vulkan/VlkImage.cpp
    src/vulkan/VlkImageView.cpp
    src/vulkan/VlkInstance.cpp
    src/vulkan/VlkPhysicalDevice.cpp
    src/vulkan/VlkPipelineLayout.cpp
    src/vulkan/VlkPipeline.cpp
    src/vulkan/VlkProxy.cpp
    src/vulkan/VlkSampler.cpp
    src/vulkan/VlkSemaphore.cpp
    src/vulkan/VlkShader.cpp
    src/vulkan/VlkShaderModule.cpp
    src/vulkan/VlkSwapchain.cpp
    src/vulkan/VlkSwapchainProperties.cpp
)

add_library(mcorevulkan ${MCOREVULKAN_SRC})
target_link_libraries(mcorevulkan PRIVATE
    Tracy::TracyClient
)
target_link_libraries(mcorevulkan PUBLIC
    ${VULKAN_LIBRARIES}
    GPUOpen::VulkanMemoryAllocator
)
target_include_directories(mcorevulkan PUBLIC
    ${VULKAN_INCLUDE_DIRS}
)

# mcorewayland

set(MCOREWAYLAND_SRC
    src/wayland/WaylandDisplay.cpp
    src/wayland/WaylandKeyboard.cpp
    src/wayland/WaylandPointer.cpp
    src/wayland/WaylandSeat.cpp
    src/wayland/WaylandWindow.cpp
)

ecm_add_wayland_client_protocol(MCOREWAYLAND_SRC
    PROTOCOL ${WAYLAND_PROTOCOLS_PKGDATADIR}/stable/xdg-shell/xdg-shell.xml
    BASENAME xdg-shell
)

ecm_add_wayland_client_protocol(MCOREWAYLAND_SRC
    PROTOCOL ${WAYLAND_PROTOCOLS_PKGDATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
    BASENAME xdg-decoration
)

ecm_add_wayland_client_protocol(MCOREWAYLAND_SRC
    PROTOCOL ${WAYLAND_PROTOCOLS_PKGDATADIR}/staging/fractional-scale/fractional-scale-v1.xml
    BASENAME fractional-scale
)

ecm_add_wayland_client_protocol(MCOREWAYLAND_SRC
    PROTOCOL ${WAYLAND_PROTOCOLS_PKGDATADIR}/stable/viewporter/viewporter.xml
    BASENAME viewporter
)

ecm_add_wayland_client_protocol(MCOREWAYLAND_SRC
    PROTOCOL ${WAYLAND_PROTOCOLS_PKGDATADIR}/staging/cursor-shape/cursor-shape-v1.xml
    BASENAME cursor-shape
)

ecm_add_wayland_client_protocol(MCOREWAYLAND_SRC
    PROTOCOL ${WAYLAND_PROTOCOLS_PKGDATADIR}/unstable/tablet/tablet-unstable-v2.xml
    BASENAME tablet
)

add_library(mcorewayland ${MCOREWAYLAND_SRC})
target_link_libraries(mcorewayland PRIVATE
    mcoreutil
    mcorevulkan
    Tracy::TracyClient
)
target_link_libraries(mcorewayland PUBLIC
    ${WAYLAND_CLIENT_LIBRARIES}
)
target_include_directories(mcorewayland PUBLIC
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
)

# mcore

set(MCORE_SRC
    src/cursor/CursorBase.cpp
    src/cursor/CursorBaseMulti.cpp
    src/cursor/CursorLogic.cpp
    src/cursor/CursorTheme.cpp
    src/cursor/WinCursor.cpp
    src/cursor/XCursor.cpp
    src/dbus/DbusMessage.cpp
    src/dbus/DbusSession.cpp
    src/backend/GpuDevice.cpp
    src/backend/drm/BackendDrm.cpp
    src/backend/drm/DrmBuffer.cpp
    src/backend/drm/DrmConnector.cpp
    src/backend/drm/DrmCrtc.cpp
    src/backend/drm/DrmDevice.cpp
    src/backend/drm/DrmPlane.cpp
    src/backend/drm/DrmProperties.cpp
    src/backend/drm/PciBus.cpp
    src/backend/wayland/BackendWayland.cpp
    src/plumbing/Display.cpp
    src/render/SoftwareCursor.cpp
    src/render/Texture.cpp
    src/server/Server.cpp
    src/MCore.cpp
)

set(SHADER_SRC
    shader/Background.frag
    shader/Background.vert
    shader/SoftwareCursor.frag
    shader/SoftwareCursor.vert
)

foreach(SHADER IN LISTS SHADER_SRC)
    get_filename_component(SPIRV ${SHADER} NAME)
    string(APPEND SPIRV ".spv")

    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSLC} -O -o ${SPIRV} ${CMAKE_CURRENT_LIST_DIR}/${SHADER}
        DEPENDS ${SHADER}
    )

    set(MCORE_SRC ${MCORE_SRC} ${SPIRV})
endforeach()

add_executable(mcore ${MCORE_SRC})
add_dependencies(mcore git-ref)
target_link_libraries(mcore PRIVATE
    mcoreutil
    mcoreimage
    mcorevulkan
    mcorewayland
    Tracy::TracyClient
    ${DISPLAYINFO_LINK_LIBRARIES}
    ${DRM_LINK_LIBRARIES}
    ${GBM_LINK_LIBRARIES}
    ${SYSTEMD_LINK_LIBRARIES}
    ${WAYLAND_SERVER_LINK_LIBRARIES}
)
target_include_directories(mcore PRIVATE
    ${DISPLAYINFO_INCLUDE_DIRS}
    ${DRM_INCLUDE_DIRS}
    ${GBM_INCLUDE_DIRS}
    ${SYSTEMD_INCLUDE_DIRS}
    ${WAYLAND_SERVER_INCLUDE_DIRS}
)

# afiv

set(AFIV_SRC
    src/tools/afiv/afiv.cpp
    src/render/Texture.cpp
)

add_executable(afiv ${AFIV_SRC})
target_link_libraries(afiv PRIVATE
    mcoreutil
    mcoreimage
    mcorevulkan
    mcorewayland
    Tracy::TracyClient
)

# vv

set(VV_SRC
    src/tools/vv/vv.cpp
)

add_executable(vv ${VV_SRC})
target_link_libraries(vv PRIVATE
    mcoreutil
    mcoreimage
    Tracy::TracyClient
)


# install

set(CMAKE_INSTALL_PREFIX /)
install(TARGETS
    mcore
    afiv
    vv
    mcoreutil
    mcoreimage
    mcorevulkan
    mcorewayland
    TracyClient
DESTINATION .)