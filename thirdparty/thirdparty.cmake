# Copyright 2014-2015, Max Planck Society.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors
#    may be used to endorse or promote products derived from this software without
#    specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
# OF THE POSSIBILITY OF SUCH DAMAGE.

# File created by Raffi Enficiaud

set(thirdparty_dir ${CMAKE_SOURCE_DIR}/thirdparty)

# the location where the archives will be deflated
set(thirdparties_deflate_directory ${CMAKE_BINARY_DIR}/external_libs_deflate)
if(NOT EXISTS ${thirdparties_deflate_directory})
  file(MAKE_DIRECTORY ${thirdparties_deflate_directory})
endif()

# custom cmake packages, should have lower priority than the ones bundled with cmake
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake_modules/ )

# these variables allow to specify to which the main project will link and
# to potentially copy some resources to the output directory of the main project.
# They are used by the CMakeLists.txt calling this file.

set(PHD_LINK_EXTERNAL)          # target to which the phd2 main library will link to
set(PHD_COPY_EXTERNAL_ALL)      # copy of a file for any configuration
set(PHD_COPY_EXTERNAL_DBG)      # copy for debug only
set(PHD_COPY_EXTERNAL_REL)      # copy for release only
set(PHD_EXTERNAL_PROJECT_DEPENDENCIES)

if(APPLE)
  # make sure not to pick up any homebrew or macports dependencies
  set(CMAKE_IGNORE_PREFIX_PATH /opt/local)
endif()

# this module will be used to find system installed libraries on Linux
if(UNIX AND NOT APPLE)
  find_package(PkgConfig)
endif()

if(WIN32)
  include(FetchContent)
  set(FETCHCONTENT_QUIET OFF)
  FetchContent_Declare(
    vcpkg
    GIT_REPOSITORY https://github.com/microsoft/vcpkg.git
    GIT_TAG f7423ee180c4b7f40d43402c2feb3859161ef625
    UPDATE_COMMAND bootstrap-vcpkg.bat -disableMetrics
    COMMAND ${CMAKE_COMMAND} -E echo "Building vcpkg cfitsio"
    COMMAND vcpkg install --binarysource=default --no-print-usage cfitsio:x86-windows
    COMMAND ${CMAKE_COMMAND} -E echo "Building vcpkg curl[ssl]"
    COMMAND vcpkg install --binarysource=default --no-print-usage curl[ssl]:x86-windows
    COMMAND ${CMAKE_COMMAND} -E echo "Building vcpkg eigen3"
    COMMAND vcpkg install --binarysource=default --no-print-usage eigen3:x86-windows
    COMMAND ${CMAKE_COMMAND} -E echo "Building vcpkg opencv4"
    COMMAND vcpkg install --binarysource=default --no-print-usage opencv4:x86-windows
  )
  message(STATUS "Preparing VCPKG")
  FetchContent_MakeAvailable(vcpkg)
  set(VCPKG_PREFIX ${vcpkg_SOURCE_DIR}/installed/x86-windows)
  set(VCPKG_DEBUG_BIN ${VCPKG_PREFIX}/debug/bin)
  set(VCPKG_RELEASE_BIN ${VCPKG_PREFIX}/bin)
  set(VCPKG_DEBUG_LIB ${VCPKG_PREFIX}/debug/lib)
  set(VCPKG_RELEASE_LIB ${VCPKG_PREFIX}/lib)
  set(VCPKG_INCLUDE ${VCPKG_PREFIX}/include)
  include_directories(${VCPKG_INCLUDE})
endif()

if(APPLE)
  find_library(quicktimeFramework      QuickTime)
  find_library(iokitFramework          IOKit)
  find_library(carbonFramework         Carbon)
  find_library(cocoaFramework          Cocoa)
  find_library(systemFramework         System)
  find_library(webkitFramework         Webkit)
  find_library(audioToolboxFramework   AudioToolbox)
  find_library(openGLFramework         OpenGL)
  find_library(coreFoundationFramework CoreFoundation)
endif()

#############################################
#
# external rules common to all platforms
#
#############################################

##############################################
# cfitsio

if(WIN32)
  include_directories(${VCPKG_INCLUDE}/cfitsio)
  list(APPEND PHD_LINK_EXTERNAL_DEBUG
      ${VCPKG_DEBUG_LIB}/cfitsio.lib
      ${VCPKG_DEBUG_LIB}/zlibd.lib
  )
  list(APPEND PHD_LINK_EXTERNAL_RELEASE
      ${VCPKG_RELEASE_LIB}/cfitsio.lib
      ${VCPKG_RELEASE_LIB}/zlib.lib
  )
  list(APPEND PHD_COPY_EXTERNAL_DBG
      ${VCPKG_DEBUG_BIN}/cfitsio.dll
      ${VCPKG_DEBUG_BIN}/zlibd1.dll
  )
  list(APPEND PHD_COPY_EXTERNAL_REL
      ${VCPKG_RELEASE_BIN}/cfitsio.dll
      ${VCPKG_RELEASE_BIN}/zlib1.dll
  )
else()
  if(APPLE)
    SET(_save_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
    SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    find_package(CFITSIO REQUIRED)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${_save_CMAKE_FIND_LIBRARY_SUFFIXES})
  else()
    find_package(CFITSIO REQUIRED)
  endif()
  include_directories(${CFITSIO_INCLUDE_DIR})
  list(APPEND PHD_LINK_EXTERNAL ${CFITSIO_LIBRARIES})
  message(STATUS "Using system's CFITSIO.")
endif()

##############################################
# VidCapture

if(WIN32)

  set(libvidcap_root ${thirdparty_dir}/VidCapture)

  # copied and adapted from the CMakeLists.txt of cfitsio project. The
  # sources of the project are left untouched

  file(GLOB VIDCAP_H_FILES "${libvidcap_root}/Source/CVCommon/*.h" "${libvidcap_root}/Source/VidCapture/*.h")

  set(VIDCAP_SRC_FILES
      Source/VidCapture/CVImage.cpp
      Source/VidCapture/CVImageGrey.cpp
      Source/VidCapture/CVImageRGB24.cpp
      Source/VidCapture/CVImageRGBFloat.cpp
      Source/VidCapture/CVVidCapture.cpp
      Source/VidCapture/CVVidCaptureDSWin32.cpp
      Source/VidCapture/CVDShowUtil.cpp
      Source/VidCapture/CVFile.cpp
      Source/VidCapture/CVPlatformWin32.cpp
      Source/VidCapture/CVTraceWin32.cpp
  )

  foreach(_src_file IN LISTS VIDCAP_SRC_FILES)
    list(APPEND VIDCAP_SRC_FILES_rooted ${libvidcap_root}/${_src_file})
  endforeach()

  add_library(VidCapture STATIC ${VIDCAP_H_FILES} ${VIDCAP_SRC_FILES_rooted})
  target_include_directories(VidCapture PUBLIC ${libvidcap_root}/Source/CVCommon ${libvidcap_root}/Source/VidCapture)

  target_compile_definitions(
    VidCapture
    PRIVATE FF_NO_UNISTD_H
    PRIVATE _CRT_SECURE_NO_WARNINGS
    PRIVATE _CRT_SECURE_NO_DEPRECATE)

  set_target_properties(VidCapture PROPERTIES
                         FOLDER "Thirdparty/")

  # indicating the link and include directives to the main project.
  # already done by the directive target_include_directories(vidcap PUBLIC
  # include_directories(${libvidcap_root})
  list(APPEND PHD_LINK_EXTERNAL VidCapture)

endif()

#############################################
# libusb: linux / apple

if(NOT WIN32)

  set(LIBUSB libusb-1.0.21)
  set(libusb_root ${thirdparties_deflate_directory}/${LIBUSB})
  set(USB_build TRUE) # indicates that the USB library is part of the project. Set to FALSE if already existing on the system
  set(LIBUSB_static TRUE)  # true for static lib, dynamic lib otherwise

  if(NOT EXISTS ${libusb_root})
    # untar the dependency
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E tar xf ${thirdparty_dir}/${LIBUSB}.tar.bz2
      WORKING_DIRECTORY ${thirdparties_deflate_directory})
  endif()

  set(libusb_dir ${libusb_root}/libusb)
  # core files
  set(libUSB_SRC
    ${libusb_dir}/core.c
    ${libusb_dir}/descriptor.c
    ${libusb_dir}/hotplug.c
    ${libusb_dir}/io.c
    ${libusb_dir}/sync.c
    ${libusb_dir}/libusb.h
    ${libusb_dir}/libusbi.h
  )

  # platform dependent files
  if(APPLE)
    list(APPEND libUSB_SRC

      # platform specific configuration file
      ${thirdparty_dir}/include/${LIBUSB}

      # platform specific implementation
      ${libusb_dir}/os/darwin_usb.h
      ${libusb_dir}/os/darwin_usb.c

      ${libusb_dir}/os/threads_posix.h
      ${libusb_dir}/os/threads_posix.c

      ${libusb_dir}/os/poll_posix.c
    )
    set(${LIBUSB}_additional_compile_definition "OS_DARWIN=1")
    set(${LIBUSB}_additional_include_dir ${thirdparty_dir}/include/${LIBUSB})
    # need to build a dynamic libusb since the ZWO SDK requires libusb
    set(LIBUSB_static FALSE)
  elseif(WIN32)
    list(APPEND libUSB_SRC

      # platform specific configuration files
      ${libusb_root}/msvc/stdint.h
      ${libusb_root}/msvc/inttypes.h
      ${libusb_root}/msvc/config.h

      # platform specific implementation
      ${libusb_dir}/os/windows_winusb.h
      ${libusb_dir}/os/windows_winusb.c

      ${libusb_dir}/os/threads_windows.h
      ${libusb_dir}/os/threads_windows.c

      ${libusb_dir}/os/poll_windows.h
      ${libusb_dir}/os/poll_windows.c
    )
    set(${LIBUSB}_additional_compile_definition "OS_WINDOWS=1")
    set(${LIBUSB}_additional_include_dir ${libusb_root}/msvc/)
  elseif(UNIX)
    # libUSB is already an indirect requirement/dependency for phd2 (through libindi).
    # I (Raffi) personally prefer having the same version
    # for all platforms, but it should in theory always be better to link against existing libraries
    # compiled and shipped by skilled people.

    # this would find the libUSB module that is installed on the system.
    # It requires "sudo apt-get install libusb-1.0-0-dev"
    if(USE_SYSTEM_LIBUSB)
      pkg_check_modules(USB_pkg libusb-1.0)
      include_directories(${USB_pkg_INCLUDE_DIRS})
      list(APPEND PHD_LINK_EXTERNAL ${USB_pkg_LIBRARIES})
      set(USB_build FALSE)
      set(usb_openphd ${USB_pkg_LIBRARIES})
      message(STATUS "Using system's libUSB.")
    else(USE_SYSTEM_LIBUSB)

      # in case the library is not installed on the system (as I have on my machines)
      # try by building the library ourselves

      list(APPEND libUSB_SRC

        # platform specific configuration file
        ${thirdparty_dir}/include/${LIBUSB}

        # platform specific implementation
        ${libusb_dir}/os/linux_usbfs.c
        ${libusb_dir}/os/linux_usbfs.h
        ${libusb_dir}/os/linux_netlink.c

        ${libusb_dir}/os/threads_posix.h
        ${libusb_dir}/os/threads_posix.c

        ${libusb_dir}/os/poll_posix.c
      )

      set(${LIBUSB}_additional_compile_definition "OS_LINUX=1")
      set(${LIBUSB}_additional_include_dir ${thirdparty_dir}/include/${LIBUSB})
    endif(USE_SYSTEM_LIBUSB)

  else()
    message(FATAL_ERROR "libUSB unsupported platform")
  endif()

  if(${USB_build})
    include_directories(${libusb_dir})

    # libUSB compilation if OSX or Win32 or not installed on Linux
    if(LIBUSB_static)
      add_library(usb_openphd ${libUSB_SRC})
    else()
      add_library(usb_openphd SHARED ${libUSB_SRC})
      # target_link_options requires newer cmake, so fall back to the old LINK_FLAGS target property
      #   target_link_options(usb_openphd -compatibility_version 2 -current_version 2)
      set_target_properties(usb_openphd PROPERTIES LINK_FLAGS "-compatibility_version 2 -current_version 2")
      target_link_libraries(usb_openphd
        ${coreFoundationFramework}
        ${iokitFramework}
        ${libUSB_link_objc}
      )
    endif()
    target_include_directories(usb_openphd PRIVATE ${${LIBUSB}_additional_include_dir})
    target_compile_definitions(usb_openphd PUBLIC ${${LIBUSB}_additional_compile_definition})
    set_property(TARGET usb_openphd PROPERTY FOLDER "Thirdparty/")

    if(WIN32)
      # silence the warnings on externals for win32
      target_compile_definitions(usb_openphd PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
      target_compile_definitions(usb_openphd PRIVATE LIBUSB_DESCRIBE "")
    endif()
    list(APPEND PHD_LINK_EXTERNAL usb_openphd)
  endif()

endif() # NOT WIN32

#############################################
# libcurl
#############################################

if(WIN32)
  list(APPEND PHD_LINK_EXTERNAL_DEBUG
      ${VCPKG_DEBUG_LIB}/libcurl-d.lib
  )
  list(APPEND PHD_LINK_EXTERNAL_RELEASE
      ${VCPKG_RELEASE_LIB}/libcurl.lib
  )
  list(APPEND PHD_COPY_EXTERNAL_DBG
      ${VCPKG_DEBUG_BIN}/libcurl-d.dll
  )
  list(APPEND PHD_COPY_EXTERNAL_REL
      ${VCPKG_RELEASE_BIN}/libcurl.dll
  )
else()
  if(APPLE)
    find_library(CURL_LIBRARIES
                 NAMES curl
                 PATHS /usr/lib
    )
    if(NOT CURL_LIBRARIES)
      message(FATAL_ERROR "libcurl not found")
    endif()
    set(CURL_FOUND True)
    set(CURL_INCLUDE_DIRS /usr/include)
  else()
    find_package(CURL REQUIRED)
  endif()
  message(STATUS "using libcurl ${CURL_LIBRARIES}")
  include_directories(${CURL_INCLUDE_DIRS})
  list(APPEND PHD_LINK_EXTERNAL ${CURL_LIBRARIES})
endif()

#############################################
# the Eigen library, mostly header only

if(WIN32)
  set(EIGEN_SRC ${VCPKG_INCLUDE}/eigen3)
else()
  find_package(Eigen3 REQUIRED)
  set(EIGEN_SRC ${EIGEN3_INCLUDE_DIR})
  message(STATUS "Using system's Eigen3.")
endif()

#############################################
# Google test
# https://github.com/google/googletest/tree/main/googletest#incorporating-into-an-existing-cmake-project

if(USE_SYSTEM_GTEST)
  find_package(GTest REQUIRED)
  message(STATUS "Using system's gtest")
else()
  include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
      # Compiling on Mac OSX Sonoma requires v1.15.2 - Comment the URL line above and uncomment URL line below for mac compilation
      #URL https://github.com/google/googletest/releases/download/v1.15.2/googletest-1.15.2.tar.gz
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

#############################################
# wxWidgets
#
# The usage is a bit different on all the platforms. For having
#  version >= 3.0, a version of cmake >= 3.0 should be used on Windows
#  on Linux/OSX it works properly this way).

set(wxWidgets_PREFIX_DIRECTORY $ENV{WXWIN} CACHE PATH "wxWidgets directory")

if(WIN32)
  # wxWidgets
  set(wxWidgets_CONFIGURATION msw CACHE STRING "Set wxWidgets configuration")

  if(NOT wxWidgets_PREFIX_DIRECTORY OR NOT EXISTS ${wxWidgets_PREFIX_DIRECTORY})
    message(FATAL_ERROR "The variable wxWidgets_PREFIX_DIRECTORY should be defined and should point to a valid wxWindows installation path. See the open-phd-guiding wiki for more information.")
  endif()

  set(wxWidgets_ROOT_DIR ${wxWidgets_PREFIX_DIRECTORY})
  set(wxWidgets_LIB_DIR ${wxWidgets_ROOT_DIR}/lib/vc_lib)
  set(wxWidgets_USE_STATIC ON)
  set(wxWidgets_USE_DEBUG ON)
  set(wxWidgets_USE_UNICODE OFF)
  find_package(wxWidgets REQUIRED COMPONENTS propgrid base core aui adv html net)
  include(${wxWidgets_USE_FILE})

elseif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  if(NOT DEFINED wxWidgets_PREFIX_DIRECTORY)
    set(wxWidgets_PREFIX_DIRECTORY "/usr/local")
  endif()
  set(wxWidgets_CONFIG_OPTIONS --prefix=${wxWidgets_PREFIX_DIRECTORY})

  find_program(wxWidgets_CONFIG_EXECUTABLE
    NAMES "wxgtk3u-3.1-config"
    PATHS ${wxWidgets_PREFIX_DIRECTORY}/bin NO_DEFAULT_PATH)
  if(NOT wxWidgets_CONFIG_EXECUTABLE)
    message(FATAL_ERROR "Cannot find wxWidgets_CONFIG_EXECUTABLE from the given directory ${wxWidgets_PREFIX_DIRECTORY}")
  endif()

  set(wxRequiredLibs aui core base adv html net)
  execute_process(COMMAND ${wxWidgets_CONFIG_EXECUTABLE} --libs ${wxRequiredLibs}
                  OUTPUT_VARIABLE wxWidgets_LIBRARIES
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
  separate_arguments(${wxWidgets_LIBRARIES})
  execute_process(COMMAND ${wxWidgets_CONFIG_EXECUTABLE} --cflags ${wxRwxRequiredLibs}
                  OUTPUT_VARIABLE wxWidgets_CXXFLAGS
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
  separate_arguments(wxWidgets_CXX_FLAGS UNIX_COMMAND "${wxWidgets_CXXFLAGS}")
  separate_arguments(wxWidgets_LDFLAGS UNIX_COMMAND "${wxWidgets_LDFLAGS}")
else()
  if(wxWidgets_PREFIX_DIRECTORY)
    set(wxWidgets_CONFIG_OPTIONS --prefix=${wxWidgets_PREFIX_DIRECTORY})

    find_program(wxWidgets_CONFIG_EXECUTABLE NAMES "wx-config" PATHS ${wxWidgets_PREFIX_DIRECTORY}/bin NO_DEFAULT_PATH)
    if(NOT wxWidgets_CONFIG_EXECUTABLE)
      message(FATAL_ERROR "Cannot find wxWidgets_CONFIG_EXECUTABLE from the given directory ${wxWidgets_PREFIX_DIRECTORY}")
    endif()
  endif()

  find_package(wxWidgets REQUIRED COMPONENTS aui core base adv html net)
  if(NOT wxWidgets_FOUND)
    message(FATAL_ERROR "wxWidgets cannot be found. Please use wx-config prefix")
  endif()
endif()

list(APPEND PHD_LINK_EXTERNAL ${wxWidgets_LIBRARIES})

#############################################
#
#  INDI
#
#############################################

if(USE_SYSTEM_LIBINDI)
  message(STATUS "Using system's libindi")
  find_package(INDI 2.0.0 REQUIRED)
  list(APPEND PHD_LINK_EXTERNAL ${INDI_CLIENT_LIBRARIES})

  find_package(ZLIB REQUIRED)
  list(APPEND PHD_LINK_EXTERNAL ${ZLIB_LIBRARIES})

  find_package(Nova REQUIRED)
  add_definitions("-DLIBNOVA")
  include_directories(${NOVA_INCLUDE_DIR})
  list(APPEND PHD_LINK_EXTERNAL ${NOVA_LIBRARIES})
else()
  include(ExternalProject)
  set(indi_INSTALL_DIR ${CMAKE_BINARY_DIR}/libindi)
  ExternalProject_Add(
    indi
    GIT_REPOSITORY https://github.com/indilib/indi.git
    GIT_TAG 856ac85b965177d23cd0c819a49fd50bdaeece60  # v2.0.5
    GIT_SHALLOW 1
    CMAKE_ARGS -Wno-dev
      -DINDI_BUILD_SERVER=OFF
      -DINDI_BUILD_DRIVERS=OFF
      -DINDI_BUILD_CLIENT=ON
      -DINDI_BUILD_QT5_CLIENT=OFF
      -DINDI_BUILD_SHARED=OFF
      -DCMAKE_PREFIX_PATH=${VCPKG_PREFIX}
      -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libindi
      -DCMAKE_CXX_FLAGS=-D_CRT_SECURE_NO_WARNINGS
      -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
  )
  include_directories(${indi_INSTALL_DIR}/include)
  if (WIN32)
    list(APPEND PHD_LINK_EXTERNAL ${indi_INSTALL_DIR}/lib/indiclient.lib)
  else()
    list(APPEND PHD_LINK_EXTERNAL ${indi_INSTALL_DIR}/lib/libindiclient.a)
    if(APPLE)
      # MacOS must use a static libnova to avoid introducing a homebrew or macports dylib dependency
      find_library(LIBNOVA REQUIRED NAMES libnova.a PATHS /usr/local/lib)
      list(APPEND PHD_LINK_EXTERNAL ${LIBNOVA})
    else()
      find_library(LIBNOVA REQUIRED NAMES nova)
      list(APPEND PHD_LINK_EXTERNAL ${LIBNOVA} z)
    endif()
    ## Define LIBNOVA when building Indi from source.
    add_definitions("-DLIBNOVA")
  endif()
  # adding indi as a dependency allows a developer to build phd2 in
  # the IDE without explicitly building anything else first, but this
  # slows down incremental development
  # list(APPEND PHD_EXTERNAL_PROJECT_DEPENDENCIES indi)
endif()

#############################################
#
# Windows specific dependencies
# - Visual Leak Detector (optional)
# - OpenCV
# - Video For Windows (vfw)
# - ASCOM camera stuff
#############################################

if(WIN32)

  if(NOT DISABLE_VLD)
    find_path(VLD_INCLUDE vld.h
        HINTS "C:/Program Files (x86)/Visual Leak Detector" ENV VLD_DIR
        PATH_SUFFIXES include
    )
    if (VLD_INCLUDE)
      get_filename_component(VLD_ROOT ${VLD_INCLUDE} DIRECTORY)
      add_definitions(-DHAVE_VLD=1)
      message(STATUS "Enabling VLD (${VLD_ROOT})")
    else()
      message(STATUS "Disabling VLD: VLD not found")
    endif()
  else()
    message(STATUS "Disabling VLD: DISABLE_VLD is set")
  endif()

  include_directories(${VCPKG_INCLUDE}/opencv2)
  list(APPEND PHD_LINK_EXTERNAL_DEBUG
      ${VCPKG_DEBUG_LIB}/opencv_imgproc4d.lib
      ${VCPKG_DEBUG_LIB}/opencv_highgui4d.lib
      ${VCPKG_DEBUG_LIB}/opencv_core4d.lib
      ${VCPKG_DEBUG_LIB}/opencv_videoio4d.lib
      ${VCPKG_DEBUG_LIB}/opencv_imgcodecs4d.lib
  )
  list(APPEND PHD_LINK_EXTERNAL_RELEASE
      ${VCPKG_RELEASE_LIB}/opencv_imgproc4.lib
      ${VCPKG_RELEASE_LIB}/opencv_highgui4.lib
      ${VCPKG_RELEASE_LIB}/opencv_core4.lib
      ${VCPKG_RELEASE_LIB}/opencv_videoio4.lib
      ${VCPKG_RELEASE_LIB}/opencv_imgcodecs4.lib
  )
  list(APPEND PHD_COPY_EXTERNAL_DBG
      ${VCPKG_DEBUG_BIN}/opencv_imgproc4d.dll
      ${VCPKG_DEBUG_BIN}/opencv_highgui4d.dll
      ${VCPKG_DEBUG_BIN}/opencv_core4d.dll
      ${VCPKG_DEBUG_BIN}/opencv_videoio4d.dll
      ${VCPKG_DEBUG_BIN}/opencv_imgcodecs4d.dll
      ${VCPKG_DEBUG_BIN}/jpeg62.dll
      ${VCPKG_DEBUG_BIN}/libpng16d.dll
      ${VCPKG_DEBUG_BIN}/tiffd.dll
      ${VCPKG_DEBUG_BIN}/liblzma.dll
      ${VCPKG_DEBUG_BIN}/libwebp.dll
      ${VCPKG_DEBUG_BIN}/libwebpdecoder.dll
      ${VCPKG_DEBUG_BIN}/libsharpyuv.dll
  )
  list(APPEND PHD_COPY_EXTERNAL_REL
      ${VCPKG_RELEASE_BIN}/opencv_imgproc4.dll
      ${VCPKG_RELEASE_BIN}/opencv_highgui4.dll
      ${VCPKG_RELEASE_BIN}/opencv_core4.dll
      ${VCPKG_RELEASE_BIN}/opencv_videoio4.dll
      ${VCPKG_RELEASE_BIN}/opencv_imgcodecs4.dll
      ${VCPKG_RELEASE_BIN}/jpeg62.dll
      ${VCPKG_RELEASE_BIN}/libpng16.dll
      ${VCPKG_RELEASE_BIN}/tiff.dll
      ${VCPKG_RELEASE_BIN}/liblzma.dll
      ${VCPKG_RELEASE_BIN}/libwebp.dll
      ${VCPKG_RELEASE_BIN}/libwebpdecoder.dll
      ${VCPKG_RELEASE_BIN}/libsharpyuv.dll
  )
endif()

if (NOT OPENSOURCE_ONLY)
  include(FetchContent)
  FetchContent_Declare(
    OGMAcamSDK
    GIT_REPOSITORY https://github.com/OGMAvision/OGMAcamSDK.git
    GIT_TAG 5e83d81384479b1684b97e9f6936e453050da4cb
    GIT_SHALLOW 1
  )
  FetchContent_MakeAvailable(OGMAcamSDK)
  include_directories(${ogmacamsdk_SOURCE_DIR}/inc)
  if (WIN32)
    list(APPEND PHD_LINK_EXTERNAL ${ogmacamsdk_SOURCE_DIR}/win/x86/ogmacam.lib)
    list(APPEND PHD_COPY_EXTERNAL_ALL ${ogmacamsdk_SOURCE_DIR}/win/x86/ogmacam.dll)
  endif()
endif()

# Various camera libraries
if(WIN32)
  # Video for Windows, directshow and windows media
  list(APPEND PHD_LINK_EXTERNAL
    vfw32.lib
    Strmiids.lib
    Quartz.lib
    winmm.lib
  )

  # gpusb
  list(APPEND PHD_LINK_EXTERNAL ${PHD_PROJECT_ROOT_DIR}/cameras/ShoestringGPUSB_DLL.lib)
  list(APPEND PHD_COPY_EXTERNAL_ALL ${PHD_PROJECT_ROOT_DIR}/WinLibs/ShoestringGPUSB_DLL.dll)
  list(APPEND PHD_LINK_EXTERNAL ${PHD_PROJECT_ROOT_DIR}/cameras/ShoestringLXUSB_DLL.lib)
  list(APPEND PHD_COPY_EXTERNAL_ALL ${PHD_PROJECT_ROOT_DIR}/WinLibs/ShoestringLXUSB_DLL.dll)

  # ASI cameras
  list(APPEND PHD_LINK_EXTERNAL ${PHD_PROJECT_ROOT_DIR}/cameras/ASICamera2.lib)
  list(APPEND PHD_COPY_EXTERNAL_ALL ${PHD_PROJECT_ROOT_DIR}/WinLibs/ASICamera2.dll)

  # ToupTek cameras
  list(APPEND PHD_LINK_EXTERNAL ${PHD_PROJECT_ROOT_DIR}/cameras/toupcam.lib)
  list(APPEND PHD_COPY_EXTERNAL_ALL ${PHD_PROJECT_ROOT_DIR}/WinLibs/toupcam.dll)

  # QHY cameras
  list(APPEND PHD_LINK_EXTERNAL ${PHD_PROJECT_ROOT_DIR}/cameras/qhyccd.lib)
  list(APPEND PHD_COPY_EXTERNAL_ALL ${PHD_PROJECT_ROOT_DIR}/WinLibs/qhyccd.dll)
  list(APPEND PHD_COPY_EXTERNAL_ALL ${PHD_PROJECT_ROOT_DIR}/WinLibs/tbb.dll)

  # altair cameras
  list(APPEND PHD_COPY_EXTERNAL_ALL ${PHD_PROJECT_ROOT_DIR}/WinLibs/altaircam.dll)
  list(APPEND PHD_COPY_EXTERNAL_ALL ${PHD_PROJECT_ROOT_DIR}/WinLibs/AltairCam_legacy.dll)

  # SBIGUDrv
  add_definitions(-DHAVE_SBIG_CAMERA=1)
  list(APPEND PHD_LINK_EXTERNAL ${PHD_PROJECT_ROOT_DIR}/cameras/SBIGUDrv.lib)
  #list(APPEND PHD_COPY_EXTERNAL_ALL ${PHD_PROJECT_ROOT_DIR}/WinLibs/SBIGUDrv.dll) # this is delay load, the dll does not exist in the sources

  # DICAMSDK
  list(APPEND PHD_LINK_EXTERNAL ${PHD_PROJECT_ROOT_DIR}/cameras/DICAMSDK.lib)
  list(APPEND PHD_COPY_EXTERNAL_ALL ${PHD_PROJECT_ROOT_DIR}/WinLibs/DICAMSDK.dll)

  # SXUSB
  list(APPEND PHD_LINK_EXTERNAL ${PHD_PROJECT_ROOT_DIR}/cameras/SXUSB.lib)
  list(APPEND PHD_COPY_EXTERNAL_ALL ${PHD_PROJECT_ROOT_DIR}/WinLibs/SXUSB.dll)

  # astroDLL
  #list(APPEND PHD_LINK_EXTERNAL ${PHD_PROJECT_ROOT_DIR}/cameras/astroDLLQHY5V.lib)
  list(APPEND PHD_COPY_EXTERNAL_ALL
    ${PHD_PROJECT_ROOT_DIR}/WinLibs/astroDLLGeneric.dll
    ${PHD_PROJECT_ROOT_DIR}/WinLibs/astroDLLQHY5V.dll
    ${PHD_PROJECT_ROOT_DIR}/WinLibs/astroDLLsspiag.dll
  )

  # CMOSDLL
  list(APPEND PHD_LINK_EXTERNAL ${PHD_PROJECT_ROOT_DIR}/cameras/CMOSDLL.lib)
  list(APPEND PHD_COPY_EXTERNAL_ALL ${PHD_PROJECT_ROOT_DIR}/WinLibs/CMOSDLL.dll)

  # inpout32 ?
  list(APPEND PHD_LINK_EXTERNAL ${PHD_PROJECT_ROOT_DIR}/cameras/inpout32.lib)
  list(APPEND PHD_COPY_EXTERNAL_ALL ${PHD_PROJECT_ROOT_DIR}/WinLibs/inpout32.dll)

  # some other that are explicitly loaded at runtime
  list(APPEND PHD_COPY_EXTERNAL_ALL
    ${PHD_PROJECT_ROOT_DIR}/WinLibs/SSPIAGCAM.dll
    ${PHD_PROJECT_ROOT_DIR}/WinLibs/SSPIAGUSB_WIN.dll
  )

  # SVB cameras
  list(APPEND PHD_LINK_EXTERNAL ${PHD_PROJECT_ROOT_DIR}/cameras/SVBCameraSDK.lib)
  list(APPEND PHD_COPY_EXTERNAL_ALL ${PHD_PROJECT_ROOT_DIR}/WinLibs/SVBCameraSDK.dll)

  # Moravian gX-driver cameras
  list(APPEND PHD_LINK_EXTERNAL
    # ${PHD_PROJECT_ROOT_DIR}/cameras/moravian/win/lib/gXeth.lib
    ${PHD_PROJECT_ROOT_DIR}/cameras/moravian/win/lib/gXusb.lib
  )
  list(APPEND PHD_COPY_EXTERNAL_ALL
    # ${PHD_PROJECT_ROOT_DIR}/cameras/moravian/win/lib/gXeth.dll
    ${PHD_PROJECT_ROOT_DIR}/cameras/moravian/win/lib/gXusb.dll
  )
  include_directories(${PHD_PROJECT_ROOT_DIR}/cameras/moravian/include)

  # Player One cameras
  list(APPEND PHD_LINK_EXTERNAL ${PHD_PROJECT_ROOT_DIR}/cameras/playerone/win/PlayerOneCamera.lib)
  list(APPEND PHD_COPY_EXTERNAL_ALL ${PHD_PROJECT_ROOT_DIR}/cameras/playerone/win/PlayerOneCamera.dll)
  include_directories(${PHD_PROJECT_ROOT_DIR}/cameras/playerone/include)

  list(APPEND PHD_COPY_EXTERNAL_ALL
    ${PHD_PROJECT_ROOT_DIR}/WinLibs/msvcr120.dll
    ${PHD_PROJECT_ROOT_DIR}/WinLibs/msvcp140.dll
    ${PHD_PROJECT_ROOT_DIR}/WinLibs/vcomp140.dll
    ${PHD_PROJECT_ROOT_DIR}/WinLibs/vcruntime140.dll
    ${PHD_PROJECT_ROOT_DIR}/WinLibs/concrt140.dll
  )

endif()

#############################################
# SBIG specific dependencies if installed part of system
#############################################
if(SBIG_SYSTEM AND UNIX)

  # Assumes SBIG's Universal driver has been loaded into the system and placed
  # in a standard path. (e.g. sbigudrv.h in /usr/include, libsbigudrv.so in /usr/lib )
  #
  # SDK for linux can be found here -> ftp://ftp.diffractionlimited.com/pub/devsw/LinuxDevKit.tar.gz
  #
  # To rebuild libSBIG
  # cd ${WORKDIR}/LinuxDevKit/x86/c/testapp
  #  local sharedlink="-shared -Wl,-soname,libSBIG-1.33.0"
  #  g++ -c ${CXXFLAGS} -c -fPIC -I /usr/include/libusb-1.0 -I ${WORKDIR}/LinuxDevKit/x86/c/testapp csbigimg.cpp csbigcam.cpp
  #  g++ -L ${WORKDIR}/LinuxDevKit/x86/c/lib64/ ${sharedlink} -o libSBIG=1.33.0 csbigimg.o csbigcam.o -lm -lsbigudrv -lusb -lcfitsio
  #  ar -cvq libSBIG.a csbigimg.o csbigcam.o

  add_definitions(-DHAVE_SBIG_CAMERA=1)
  add_definitions("-DTARGET=7")
  message(STATUS "Finding SBIG Univeral Drivers on system")
  find_path(SBIG_INCLUDE_DIR sbigudrv.h)
  find_library(SBIG_LIBRARIES NAMES SBIG)
  find_library(SBIGUDRV_LIBRARIES NAMES sbigudrv)
  include_directories(${SBIG_INCLUDE_DIR})

  list(APPEND PHD_LINK_EXTERNAL
    SBIG
    sbigudrv
  )

endif()

#############################################
#
# OSX specific dependencies
#
#############################################
if(APPLE)
  list(APPEND PHD_LINK_EXTERNAL
    ${QuickTime}
    ${IOKit}
    ${Carbon}
    ${Cocoa}
    ${System}
    ${Webkit}
    ${AudioToolbox}
    ${OpenGL}
  )

  find_path(CARBON_INCLUDE_DIR Carbon.h)

  #############################################
  # Camera frameworks
  #
  find_library( sbigudFramework
                NAMES SBIGUDrv
                PATHS ${thirdparty_dir}/frameworks)
  add_definitions(-DHAVE_SBIG_CAMERA=1)
  if(NOT sbigudFramework)
    message(FATAL_ERROR "Cannot find the SBIGUDrv drivers")
  endif()
  include_directories(${sbigudFramework})
  add_definitions(-DHAVE_SBIG_CAMERA=1)
  list(APPEND PHD_LINK_EXTERNAL ${sbigudFramework})
  list(APPEND phd2_OSX_FRAMEWORKS ${sbigudFramework})

  find_library( asiCamera2
                NAMES ASICamera2
                PATHS ${PHD_PROJECT_ROOT_DIR}/cameras/zwolibs/mac)
  if(NOT asiCamera2)
    message(FATAL_ERROR "Cannot find the asiCamera2 drivers")
  endif()
  add_definitions(-DHAVE_ZWO_CAMERA=1)
  list(APPEND PHD_LINK_EXTERNAL ${asiCamera2})
  list(APPEND phd2_OSX_FRAMEWORKS ${asiCamera2})

  find_library( SVBCameraSDK
                NAMES SVBCameraSDK
                PATHS ${PHD_PROJECT_ROOT_DIR}/cameras/svblibs/mac/x64)
  if(NOT SVBCameraSDK)
    message(FATAL_ERROR "Cannot find the Svbony SDK libs")
  endif()

  if(SVBCameraSDK)
    add_definitions(-DHAVE_SVB_CAMERA=1)
    list(APPEND PHD_LINK_EXTERNAL ${SVBCameraSDK})
    list(APPEND phd2_OSX_FRAMEWORKS ${SVBCameraSDK})
  endif()

  find_library( qhylib
                NAMES qhyccd
                PATHS ${PHD_PROJECT_ROOT_DIR}/cameras/qhyccdlibs/mac/universal)
  if(NOT qhylib)
    message(FATAL_ERROR "Cannot find the qhy SDK libs")
  endif()
  add_definitions(-DHAVE_QHY_CAMERA=1)
  list(APPEND PHD_LINK_EXTERNAL ${qhylib})
  list(APPEND phd2_OSX_FRAMEWORKS ${qhylib})

  find_library( toupcam
                NAMES toupcam
                PATHS ${PHD_PROJECT_ROOT_DIR}/cameras/toupcam/mac)
  if(NOT toupcam)
    message(FATAL_ERROR "Cannot find the toupcam drivers")
  endif()
  list(APPEND PHD_LINK_EXTERNAL ${toupcam})
  add_definitions(-DHAVE_TOUPTEK_CAMERA=1)
  list(APPEND phd2_OSX_FRAMEWORKS ${toupcam})

  find_library( ogmacam
                NAMES ogmacam
                PATHS ${ogmacamsdk_SOURCE_DIR}/mac/x64+x86)
  if(NOT ogmacam)
    message(FATAL_ERROR "Cannot find the ogmacam drivers")
  endif()
  list(APPEND PHD_LINK_EXTERNAL ${ogmacam})
  add_definitions(-DHAVE_OGMA_CAMERA=1)
  list(APPEND phd2_OSX_FRAMEWORKS ${ogmacam})

  set(LIBOPENSSAG openssag)
  set(libopenssag_dir ${thirdparty_dir}/${LIBOPENSSAG}/src)
  include_directories(${libopenssag_dir})
  set(libOPENSSAG_SRC
    ${libopenssag_dir}/firmware.h
    ${libopenssag_dir}/loader.cpp
    ${libopenssag_dir}/openssag_priv.h
    ${libopenssag_dir}/openssag.cpp
    ${libopenssag_dir}/openssag.h
  )
  add_library(OpenSSAG ${libOPENSSAG_SRC})
  target_include_directories(OpenSSAG PRIVATE ${thirdparty_dir}/${LIBOPENSSAG}/src)
  list(APPEND PHD_LINK_EXTERNAL OpenSSAG)
  add_definitions(-DHAVE_OPENSSAG_CAMERA=1)
  set_property(TARGET OpenSSAG PROPERTY FOLDER "Thirdparty/")

  find_library( playerone
                NAMES PlayerOneCamera
                PATHS ${PHD_PROJECT_ROOT_DIR}/cameras/playerone/mac)
  if(NOT playerone)
    message(FATAL_ERROR "Cannot find the playerone SDK lib")
  endif()
  include_directories(${PHD_PROJECT_ROOT_DIR}/cameras/playerone/include)
  list(APPEND PHD_LINK_EXTERNAL ${playerone})
  add_definitions(-DHAVE_PLAYERONE_CAMERA=1)
  list(APPEND phd2_OSX_FRAMEWORKS ${playerone})

endif()  # APPLE

#############################################
#
# Unix/Linux specific dependencies
# - ASI cameras
# - USB (commonly shared)
# - math (libm)
# -
#
#############################################
if(UNIX AND NOT APPLE)

  if (NOT OPENSOURCE_ONLY)

    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^armv6(.*)")
      set(zwoarch "armv6")
      set(qhyarch "arm32")
      set(toupcam_arch "armel")
      set(svbony_arch "armv6")
      set(playerone_arch arm32)
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^armv7(.*)|arm64|aarch64|^armv8(.*)")
      if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(zwoarch "armv8")
        set(qhyarch "arm64")
        set(toupcam_arch "arm64")
        set(svbony_arch "armv8")
        set(playerone_arch arm64)
      else()
        set(zwoarch "armv7")
        set(qhyarch "arm32")
        set(toupcam_arch "armhf")
        set(svbony_arch "armv7")
        set(playerone_arch arm32)
      endif()
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "x86|X86|amd64|AMD64|i.86")
      if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(zwoarch "x64")
        set(qhyarch "x86_64")
        set(toupcam_arch "x64")
        set(svbony_arch "x64")
        set(playerone_arch x64)
      else()
        set(zwoarch "x86")
        set(qhyarch "x86_32")  # no longer distributed by QHY
        set(toupcam_arch "x86")
        set(svbony_arch "x86")
        set(playerone_arch x86)
      endif()
    else()
      message(FATAL_ERROR "unknown system architecture")
    endif()

    find_path(ZWO_INCLUDE_DIR ASICamera2.h
      NO_DEFAULT_PATHS
      PATHS ${PHD_PROJECT_ROOT_DIR}/cameras
    )

    # The binary libraries below do not support FreeBSD, ignore them
    # when building for FreeBSD.
    if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
      find_library(asiCamera2
             NAMES ASICamera2
             NO_DEFAULT_PATHS
             PATHS ${PHD_PROJECT_ROOT_DIR}/cameras/zwolibs/${zwoarch})

      if(NOT asiCamera2)
        message(FATAL_ERROR "Cannot find the asiCamera2 drivers")
      endif()
      message(STATUS "Found ASICamera2 lib ${asiCamera2}")
      add_definitions(-DHAVE_ZWO_CAMERA=1)
      list(APPEND PHD_LINK_EXTERNAL ${asiCamera2})

      find_library(toupcam
             NAMES toupcam
             NO_DEFAULT_PATHS
             NO_CMAKE_SYSTEM_PATH
             PATHS ${PHD_PROJECT_ROOT_DIR}/cameras/toupcam/linux/${toupcam_arch})
      if(NOT toupcam)
        message(FATAL_ERROR "Cannot find the toupcam drivers")
      endif()
      message(STATUS "Found toupcam lib ${toupcam}")
      add_definitions(-DHAVE_TOUPTEK_CAMERA=1)
      list(APPEND PHD_LINK_EXTERNAL ${toupcam})
      list(APPEND PHD_INSTALL_LIBS ${toupcam})

      find_library(ogmacam
             NAMES ogmacam
             NO_DEFAULT_PATHS
             NO_CMAKE_SYSTEM_PATH
             PATHS ${ogmacamsdk_SOURCE_DIR}/linux/${toupcam_arch})
      if(NOT ogmacam)
        message(FATAL_ERROR "Cannot find the ogmacam drivers")
      endif()
      message(STATUS "Found ogmacam lib ${ogmacam}")
      add_definitions(-DHAVE_OGMA_CAMERA=1)
      list(APPEND PHD_LINK_EXTERNAL ${ogmacam})
      list(APPEND PHD_INSTALL_LIBS ${ogmacam})

      find_library(SVBCameraSDK
            NAMES SVBCameraSDK
            NO_DEFAULT_PATHS
            PATHS ${PHD_PROJECT_ROOT_DIR}/cameras/svblibs/linux/${svbony_arch})
      if(NOT SVBCameraSDK)
        message(FATAL_ERROR "Cannot find the SVBCameraSDK drivers")
      endif()
      message(STATUS "Found SVBCameraSDK lib ${SVBCameraSDK}")
      add_definitions(-DHAVE_SVB_CAMERA=1)
      list(APPEND PHD_LINK_EXTERNAL ${SVBCameraSDK})
      list(APPEND PHD_INSTALL_LIBS ${SVBCameraSDK})

      if(IS_DIRECTORY ${PHD_PROJECT_ROOT_DIR}/cameras/qhyccdlibs/linux/${qhyarch})
        add_definitions(-DHAVE_QHY_CAMERA=1)

        # be careful not to pick up any other qhy lib on the system
        find_library(qhylib
               NAMES qhyccd
               NO_DEFAULT_PATH
               PATHS ${PHD_PROJECT_ROOT_DIR}/cameras/qhyccdlibs/linux/${qhyarch})
        if(NOT qhylib)
          message(FATAL_ERROR "Cannot find the qhy SDK libs")
        endif()
        list(APPEND PHD_LINK_EXTERNAL ${qhylib})
      endif()

      find_library( playerone
                    NAMES PlayerOneCamera
                    NO_DEFAULT_PATHS
                    PATHS ${PHD_PROJECT_ROOT_DIR}/cameras/playerone/linux/${playerone_arch})

      if(NOT playerone)
        message(FATAL_ERROR "Cannot find the PlayerOneCamera SDK lib")
      endif()
      message(STATUS "Found PlayerOneCamera SDK lib ${playerone}")
      include_directories(${PHD_PROJECT_ROOT_DIR}/cameras/playerone/include)
      add_definitions(-DHAVE_PLAYERONE_CAMERA=1)
      list(APPEND PHD_LINK_EXTERNAL ${playerone})
      # install the .so and symlinks
      file(GLOB playerone_so_files "${playerone}*")
      list(APPEND PHD_INSTALL_LIBS ${playerone_so_files})

    endif(NOT ${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")

    find_program(LSB_RELEASE_EXEC lsb_release)
    if(LSB_RELEASE_EXEC)
      execute_process(COMMAND ${LSB_RELEASE_EXEC} -is
        OUTPUT_VARIABLE LSB_RELEASE_ID
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        )
      execute_process(COMMAND ${LSB_RELEASE_EXEC} -rs
        OUTPUT_VARIABLE LSB_RELEASE_RELEASE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        )
      # temporarily disable qhy camera pending fix for link error on Ubuntu Trusty
      if((${LSB_RELEASE_ID} STREQUAL "Ubuntu") AND (${LSB_RELEASE_RELEASE} STREQUAL "14.04"))
        message(STATUS "Disabling QHY camera support on this platform")
        remove_definitions(-DHAVE_QHY_CAMERA=1)
      endif()
    endif()

    set(LIBOPENSSAG openssag)
    set(libopenssag_dir ${thirdparty_dir}/${LIBOPENSSAG}/src)
    include_directories(${libopenssag_dir})
    set(libOPENSSAG_SRC
      ${libopenssag_dir}/firmware.h
      ${libopenssag_dir}/loader.cpp
      ${libopenssag_dir}/openssag_priv.h
      ${libopenssag_dir}/openssag.cpp
      ${libopenssag_dir}/openssag.h
    )
    add_library(OpenSSAG ${libOPENSSAG_SRC})
    target_include_directories(OpenSSAG PRIVATE ${thirdparty_dir}/${LIBOPENSSAG}/src)
    add_definitions(-DHAVE_OPENSSAG_CAMERA=1)
    list(APPEND PHD_LINK_EXTERNAL OpenSSAG)
    set_property(TARGET OpenSSAG PROPERTY FOLDER "Thirdparty/")

  endif()  # OPENSOURCE_ONLY

  # math library is needed, and should be one of the last things to link to here
  find_library(mathlib NAMES m)
  list(APPEND PHD_LINK_EXTERNAL ${mathlib})

endif()

#############################################
# Starlight Xpress
#############################################

if(WIN32)
  add_definitions(-DHAVE_SXV_CAMERA=1)
elseif(UNIX OR APPLE)
  add_definitions(-DHAVE_SXV_CAMERA=1)
  set(SXV_PLATFORM_SRC
      ${PHD_PROJECT_ROOT_DIR}/cameras/SXMacLib.h
      ${PHD_PROJECT_ROOT_DIR}/cameras/SXMacLib.c
  )
endif()

#############################################
# KwiqGuider
#############################################

if(APPLE)
  add_definitions(-DHAVE_KWIQGUIDER_CAMERA=1)
  include_directories(${PHD_PROJECT_ROOT_DIR}/cameras/KWIQGuider/)
  set(KWIQGuider_PLATFORM_SRC
    ${PHD_PROJECT_ROOT_DIR}/cameras/KWIQGuider/KWIQGuider.cpp
    ${PHD_PROJECT_ROOT_DIR}/cameras/KWIQGuider/KWIQGuider.h
    ${PHD_PROJECT_ROOT_DIR}/cameras/KWIQGuider/KWIQGuider_firmware.h
    ${PHD_PROJECT_ROOT_DIR}/cameras/KWIQGuider/KWIQGuider_loader.cpp
    ${PHD_PROJECT_ROOT_DIR}/cameras/KWIQGuider/KWIQGuider_priv.h
  )
endif()

#############################################
#
# gettext and msgmerge tools for documentation/internationalization
#
#############################################

# zip file support integrated in cmake 3.2+
if(WIN32 AND ("${CMAKE_VERSION}" VERSION_GREATER "3.2")
         AND ("${GETTEXT_ROOT}" STREQUAL ""))

  # GETTEXT_ROOT not given from the command line: deflating our own

  set(GETTEXTTOOLS gettext-0.14.4)
  set(GETTEXT_ROOT ${thirdparties_deflate_directory}/${GETTEXTTOOLS})

  # deflate
  if(NOT EXISTS ${GETTEXT_ROOT})

    message(STATUS "Deflating gettexttools from thirdparties to ${GETTEXT_ROOT}")
    # create directory
    if(NOT EXISTS ${GETTEXT_ROOT})
      file(MAKE_DIRECTORY ${GETTEXT_ROOT})
    endif()

    # untar the dependency
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf ${thirdparty_dir}/${GETTEXTTOOLS}-bin.zip
      WORKING_DIRECTORY ${GETTEXT_ROOT})
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf ${thirdparty_dir}/${GETTEXTTOOLS}-dep.zip
      WORKING_DIRECTORY ${GETTEXT_ROOT})
  endif()

endif()

set(GETTEXT_FINDPROGRAM_OPTIONS)
if(NOT ("${GETTEXT_ROOT}" STREQUAL ""))
  set(GETTEXT_FINDPROGRAM_OPTIONS
      PATHS ${GETTEXT_ROOT}
               PATH_SUFFIXES bin
               DOC "gettext program deflated from the thirdparties"
               NO_DEFAULT_PATH)
endif()

find_program(XGETTEXT
             NAMES xgettext
             ${GETTEXT_FINDPROGRAM_OPTIONS})

find_program(MSGFMT
              NAMES msgfmt
             ${GETTEXT_FINDPROGRAM_OPTIONS})

find_program(MSGMERGE
              NAMES msgmerge
             ${GETTEXT_FINDPROGRAM_OPTIONS})

if(NOT XGETTEXT)
  message(STATUS "'xgettext' program not found")
else()
  message(STATUS "'xgettext' program found at '${XGETTEXT}'")
endif()

if(NOT MSGFMT)
  message(STATUS "'msgfmt' program not found")
else()
  message(STATUS "'msgfmt' program found at '${MSGFMT}'")
endif()

if(NOT MSGMERGE)
  message(STATUS "'msgmerge' program not found")
else()
  message(STATUS "'msgmerge' program found at '${MSGMERGE}'")
endif()
