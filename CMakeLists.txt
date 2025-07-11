cmake_minimum_required(VERSION 3.29)

option(TRACY_ENABLE "Enable Tracy" OFF)
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option(MARCH_NATIVE "Enable -march=native" ON)
option(SANITIZE "Sanitizer parameters" OFF)
option(BUILD_MCORE "Build defunct mcore binary" OFF)

set(CMAKE_CXX_STANDARD 20)

project(ModernCore
    LANGUAGES C CXX
)

include(CheckCompilerFlag)
include(FindPkgConfig)
include(cmake/config.cmake)
include(cmake/CPM.cmake)
include(cmake/FindWaylandScanner.cmake)

if(NOT TRACY_ENABLE)
    set(TRACY_OPTIONS "TRACY_STATIC ON")
endif()

if(SANITIZE)
    add_compile_options(-fsanitize=${SANITIZE} -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${SANITIZE} -fno-omit-frame-pointer)
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
    GIT_TAG v3.3.0
    EXCLUDE_FROM_ALL TRUE
)

CPMAddPackage(
    NAME stb
    GITHUB_REPOSITORY nothings/stb
    GIT_TAG f58f558c120e9b32c217290b80bad1a0729fbb2c
    DOWNLOAD_ONLY TRUE
)

set(BUILD_SHARED_LIBS_SAVE ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
CPMAddPackage(
    NAME base64
    GITHUB_REPOSITORY aklomp/base64
    GIT_TAG v0.5.2
    OPTIONS
        "BASE64_BUILD_CLI OFF"
        "BASE64_WITH_OpenMP OFF"
    EXCLUDE_FROM_ALL TRUE
)
set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_SAVE})

find_program(GLSLC glslc REQUIRED)

pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(EXIF REQUIRED libexif)
pkg_check_modules(EXR REQUIRED OpenEXR)
pkg_check_modules(HEIF REQUIRED libheif)
pkg_check_modules(JPEG REQUIRED libjpeg)
pkg_check_modules(JXL REQUIRED libjxl libjxl_threads)
pkg_check_modules(LCMS REQUIRED lcms2)
pkg_check_modules(LZ4 REQUIRED liblz4)
pkg_check_modules(PNG REQUIRED libpng)
pkg_check_modules(PUGIXML REQUIRED pugixml)
pkg_check_modules(RAW REQUIRED libraw)
pkg_check_modules(RSVG REQUIRED librsvg-2.0)
pkg_check_modules(SIXEL REQUIRED libsixel)
pkg_check_modules(TIFF REQUIRED libtiff-4)
pkg_check_modules(VULKAN REQUIRED vulkan)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
pkg_check_modules(WEBP REQUIRED libwebpdemux)
pkg_check_modules(XKB REQUIRED xkbcommon)
pkg_check_modules(ZLIB REQUIRED zlib)

if(BUILD_MCORE)
    pkg_check_modules(DISPLAYINFO REQUIRED libdisplay-info)
    pkg_check_modules(DRM REQUIRED libdrm)
    pkg_check_modules(GBM REQUIRED gbm)
    pkg_check_modules(SYSTEMD REQUIRED libsystemd)
    pkg_check_modules(WAYLAND_SERVER REQUIRED wayland-server)
endif()

pkg_get_variable(WAYLAND_PROTOCOLS_PKGDATADIR wayland-protocols pkgdatadir)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/src)

if(TRACY_ENABLE)
    add_definitions(-DTRACY_ENABLE)
    add_compile_options(-g -O3 -fno-omit-frame-pointer)
endif()

if(MARCH_NATIVE)
    check_compiler_flag(CXX "-march=native" HAS_MARCH_NATIVE)
    if(HAS_MARCH_NATIVE)
        add_compile_options(-march=native)
    endif()
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_custom_target(git-ref
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/cmake/git-ref.py
    BYPRODUCTS GitRef.hpp
)

# embed

add_executable(embed helpers/embed.cpp)
target_link_libraries(embed PRIVATE ${LZ4_LINK_LIBRARIES})
target_include_directories(embed PRIVATE ${LZ4_INCLUDE_DIRS})

add_custom_command(
    OUTPUT data
    COMMAND ${CMAKE_COMMAND} -E make_directory data
)

function(Embed LIST NAME FILE)
    add_custom_command(
        OUTPUT data/${NAME}.cpp data/${NAME}.hpp
        COMMAND embed ${NAME} ${CMAKE_CURRENT_LIST_DIR}/${FILE} data/${NAME}
        DEPENDS data embed ${CMAKE_CURRENT_LIST_DIR}/${FILE}
    )
    list(APPEND ${LIST} data/${NAME}.cpp)
    return(PROPAGATE ${LIST})
endfunction()

add_custom_command(
    OUTPUT _tmp
    COMMAND ${CMAKE_COMMAND} -E make_directory _tmp
)

add_custom_command(
    OUTPUT shader
    COMMAND ${CMAKE_COMMAND} -E make_directory shader
)

function(EmbedShader LIST NAME FILE)
    add_custom_command(
        OUTPUT _tmp/${NAME}.spv
        COMMAND ${GLSLC} -MD -MF _tmp/${NAME}.spv.d --target-env=vulkan1.3 -O -o _tmp/${NAME}.spv ${CMAKE_CURRENT_LIST_DIR}/${FILE}
        DEPENDS _tmp ${GLSLC} ${CMAKE_CURRENT_LIST_DIR}/${FILE}
        DEPFILE _tmp/${NAME}.spv.d
    )
    add_custom_command(
        OUTPUT shader/${NAME}.cpp shader/${NAME}.hpp
        COMMAND embed ${NAME} _tmp/${NAME}.spv shader/${NAME}
        DEPENDS shader embed _tmp/${NAME}.spv
    )
    list(APPEND ${LIST} shader/${NAME}.cpp)
    return(PROPAGATE ${LIST})
endfunction()

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
    src/util/BitmapAnim.cpp
    src/util/BitmapHdr.cpp
    src/util/BitmapHdrHalf.cpp
    src/util/Callstack.cpp
    src/util/Config.cpp
    src/util/EmbedData.cpp
    src/util/FileBuffer.cpp
    src/util/Filesystem.cpp
    src/util/Home.cpp
    src/util/Logs.cpp
    src/util/MemoryBuffer.cpp
    src/util/TaskDispatch.cpp
    src/util/Tonemapper.cpp
    src/util/TonemapperAgx.cpp
    src/util/TonemapperPbr.cpp
    src/util/Url.cpp
    src/util/stb_image_resize_impl.cpp
)

Embed(MCOREUTIL_SRC CmykIcm src/util/cmyk.icm)

add_library(mcoreutil ${MCOREUTIL_SRC})
target_link_libraries(mcoreutil PRIVATE
    Tracy::TracyClient
    ${LZ4_LINK_LIBRARIES}
    ${PNG_LINK_LIBRARIES}
)
target_include_directories(mcoreutil PRIVATE
    ${LZ4_INCLUDE_DIRS}
    ${stb_SOURCE_DIR}
    ${PNG_INCLUDE_DIRS}
)

# mcoreimage

set(MCOREIMAGE_SRC
    src/image/bcdec.c
    src/image/DdsLoader.cpp
    src/image/ExrLoader.cpp
    src/image/HeifLoader.cpp
    src/image/ImageLoader.cpp
    src/image/JpgLoader.cpp
    src/image/JxlLoader.cpp
    src/image/PcxLoader.cpp
    src/image/PngLoader.cpp
    src/image/PvrLoader.cpp
    src/image/RawLoader.cpp
    src/image/StbImageLoader.cpp
    src/image/TiffLoader.cpp
    src/image/WebpLoader.cpp
    src/image/vector/PdfImage.cpp
    src/image/vector/SvgImage.cpp
)

add_library(mcoreimage ${MCOREIMAGE_SRC})
target_link_libraries(mcoreimage PRIVATE
    mcoreutil
    Tracy::TracyClient
    ${CAIRO_LINK_LIBRARIES}
    ${EXIF_LINK_LIBRARIES}
    ${EXR_LINK_LIBRARIES}
    ${HEIF_LINK_LIBRARIES}
    ${JPEG_LINK_LIBRARIES}
    ${JXL_LINK_LIBRARIES}
    ${LCMS_LINK_LIBRARIES}
    ${PNG_LINK_LIBRARIES}
    ${PUGIXML_LINK_LIBRARIES}
    ${RAW_LINK_LIBRARIES}
    ${RSVG_LINK_LIBRARIES}
    ${TIFF_LINK_LIBRARIES}
    ${WEBP_LINK_LIBRARIES}
)
target_include_directories(mcoreimage PRIVATE
    ${CAIRO_INCLUDE_DIRS}
    ${EXIF_INCLUDE_DIRS}
    ${EXR_INCLUDE_DIRS}
    ${HEIF_INCLUDE_DIRS}
    ${JPEG_INCLUDE_DIRS}
    ${JXL_INCLUDE_DIRS}
    ${LCMS_INCLUDE_DIRS}
    ${PNG_INCLUDE_DIRS}
    ${PUGIXML_INCLUDE_DIRS}
    ${RAW_INCLUDE_DIRS}
    ${RSVG_INCLUDE_DIRS}
    ${TIFF_INCLUDE_DIRS}
    ${WEBP_INCLUDE_DIRS}
    ${stb_SOURCE_DIR}
)

# mcorevulkan

set(MCOREVULKAN_SRC
    src/vulkan/ext/DeviceInfo.cpp
    src/vulkan/ext/PhysDevSel.cpp
    src/vulkan/ext/Texture.cpp
    src/vulkan/VlkAllocator.cpp
    src/vulkan/VlkBuffer.cpp
    src/vulkan/VlkCommandBuffer.cpp
    src/vulkan/VlkCommandPool.cpp
    src/vulkan/VlkDescriptorSetLayout.cpp
    src/vulkan/VlkDevice.cpp
    src/vulkan/VlkFence.cpp
    src/vulkan/VlkGarbage.cpp
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
    src/vulkan/VlkSurface.cpp
    src/vulkan/VlkSwapchain.cpp
    src/vulkan/VlkSwapchainProperties.cpp
)

add_library(mcorevulkan ${MCOREVULKAN_SRC})
target_link_libraries(mcorevulkan PRIVATE
    Tracy::TracyClient
)
target_link_libraries(mcorevulkan PUBLIC
    ${VULKAN_LINK_LIBRARIES}
    GPUOpen::VulkanMemoryAllocator
)
target_include_directories(mcorevulkan PUBLIC
    ${VULKAN_INCLUDE_DIRS}
)

# mcorewayland

set(MCOREWAYLAND_SRC
    src/wayland/WaylandDataOffer.cpp
    src/wayland/WaylandDisplay.cpp
    src/wayland/WaylandKeyboard.cpp
    src/wayland/WaylandOutput.cpp
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

ecm_add_wayland_client_protocol(MCOREWAYLAND_SRC
    PROTOCOL ${WAYLAND_PROTOCOLS_PKGDATADIR}/staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml
    BASENAME xdg-toplevel-icon
)

ecm_add_wayland_client_protocol(MCOREWAYLAND_SRC
    PROTOCOL ${WAYLAND_PROTOCOLS_PKGDATADIR}/staging/xdg-activation/xdg-activation-v1.xml
    BASENAME xdg-activation
)

ecm_add_wayland_client_protocol(MCOREWAYLAND_SRC
    PROTOCOL ${WAYLAND_PROTOCOLS_PKGDATADIR}/staging/color-management/color-management-v1.xml
    BASENAME color-management
)

add_library(mcorewayland ${MCOREWAYLAND_SRC})
target_link_libraries(mcorewayland PRIVATE
    mcoreimage
    mcoreutil
    mcorevulkan
    Tracy::TracyClient
)
target_link_libraries(mcorewayland PUBLIC
    ${WAYLAND_CLIENT_LINK_LIBRARIES}
    ${XKB_LINK_LIBRARIES}
)
target_include_directories(mcorewayland PUBLIC
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
    ${XKB_INCLUDE_DIRS}
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
    src/server/Server.cpp
    src/server/Display.cpp
    src/MCore.cpp
)

if(BUILD_MCORE)
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
endif()

# iv

set(IV_SRC
    src/tools/iv/Background.cpp
    src/tools/iv/BusyIndicator.cpp
    src/tools/iv/ImageProvider.cpp
    src/tools/iv/ImageView.cpp
    src/tools/iv/iv.cpp
    src/tools/iv/Viewport.cpp
)

Embed(IV_SRC HourglassSvg src/tools/iv/assets/hourglass.svg)
Embed(IV_SRC IconSvg src/tools/iv/assets/icon.svg)

EmbedShader(IV_SRC BackgroundVert src/tools/iv/shader/Background.vert)
EmbedShader(IV_SRC BackgroundFrag src/tools/iv/shader/Background.frag)
EmbedShader(IV_SRC BackgroundPqFrag src/tools/iv/shader/BackgroundPq.frag)
EmbedShader(IV_SRC BusyIndicatorVert src/tools/iv/shader/BusyIndicator.vert)
EmbedShader(IV_SRC NearestFrag src/tools/iv/shader/Nearest.frag)
EmbedShader(IV_SRC NearestPqFrag src/tools/iv/shader/NearestPq.frag)
EmbedShader(IV_SRC SupersampleFrag src/tools/iv/shader/Supersample.frag)
EmbedShader(IV_SRC SupersamplePqFrag src/tools/iv/shader/SupersamplePq.frag)
EmbedShader(IV_SRC TexturingVert src/tools/iv/shader/Texturing.vert)
EmbedShader(IV_SRC TexturingFrag src/tools/iv/shader/Texturing.frag)
EmbedShader(IV_SRC TexturingPqFrag src/tools/iv/shader/TexturingPq.frag)
EmbedShader(IV_SRC TexturingAlphaFrag src/tools/iv/shader/TexturingAlpha.frag)
EmbedShader(IV_SRC TexturingAlphaPqFrag src/tools/iv/shader/TexturingAlphaPq.frag)


add_executable(iv ${IV_SRC})
add_dependencies(iv git-ref)
target_link_libraries(iv PRIVATE
    mcoreutil
    mcoreimage
    mcorevulkan
    mcorewayland
    Tracy::TracyClient
)

# vv

set(VV_SRC
    src/tools/vv/vv.cpp
    src/tools/vv/Terminal.cpp
)

add_executable(vv ${VV_SRC})
add_dependencies(vv git-ref)
target_include_directories(vv PRIVATE
    ${SIXEL_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
)
target_link_libraries(vv PRIVATE
    mcoreutil
    mcoreimage
    Tracy::TracyClient
    base64
    ${SIXEL_LINK_LIBRARIES}
    ${ZLIB_LINK_LIBRARIES}
)


# install

install(TARGETS
    iv
    vv
    mcoreutil
    mcoreimage
    mcorevulkan
    mcorewayland
)

install(FILES src/tools/iv/desktop/iv.desktop DESTINATION share/applications)
install(FILES src/tools/iv/assets/icon.svg DESTINATION share/icons/hicolor/scalable/apps RENAME iv.svg)

if(BUILD_MCORE)
    install(TARGETS mcore)
endif()