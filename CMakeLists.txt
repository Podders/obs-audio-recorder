cmake_minimum_required(VERSION 3.24)

project(obs_audio_recorder VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(OBS_STUDIO_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/obs-studio" CACHE PATH "Path to the vendored OBS Studio source tree")

if(NOT EXISTS "${OBS_STUDIO_SOURCE_DIR}/libobs/obs.h")
  message(FATAL_ERROR "OBS Studio source tree not found at ${OBS_STUDIO_SOURCE_DIR}")
endif()

add_library(obs-audio-recorder MODULE
  src/main.cpp
)

target_include_directories(obs-audio-recorder PRIVATE
  "${OBS_STUDIO_SOURCE_DIR}/libobs"
  "${OBS_STUDIO_SOURCE_DIR}/libobs/util"
  "${OBS_STUDIO_SOURCE_DIR}/libobs/callback"
  "${OBS_STUDIO_SOURCE_DIR}/libobs/graphics"
  "${OBS_STUDIO_SOURCE_DIR}/libobs/media-io"
  "${OBS_STUDIO_SOURCE_DIR}/frontend/api"
)

target_compile_definitions(obs-audio-recorder PRIVATE
  QT_NO_VERSION_TAGGING
)

if(APPLE)
  set(QTBASE_PREFIX "/usr/local/opt/qtbase" CACHE PATH "Qt base prefix used for headers")

  file(GLOB _qt_core_header_version_dirs LIST_DIRECTORIES true
    "${QTBASE_PREFIX}/lib/QtCore.framework/Versions/A/Headers/[0-9]*"
  )
  list(LENGTH _qt_core_header_version_dirs _qt_core_header_version_count)
  if(_qt_core_header_version_count EQUAL 0)
    message(FATAL_ERROR "QtCore headers not found at ${QTBASE_PREFIX}")
  endif()
  list(GET _qt_core_header_version_dirs 0 QT_CORE_FRAMEWORK_HEADERS_DIR)
  get_filename_component(QT_CORE_FRAMEWORK_HEADERS_ROOT "${QT_CORE_FRAMEWORK_HEADERS_DIR}" DIRECTORY)

  file(GLOB _qt_widgets_header_version_dirs LIST_DIRECTORIES true
    "${QTBASE_PREFIX}/lib/QtWidgets.framework/Versions/A/Headers/[0-9]*"
  )
  list(LENGTH _qt_widgets_header_version_dirs _qt_widgets_header_version_count)
  if(_qt_widgets_header_version_count EQUAL 0)
    message(FATAL_ERROR "QtWidgets headers not found at ${QTBASE_PREFIX}")
  endif()
  list(GET _qt_widgets_header_version_dirs 0 QT_WIDGETS_FRAMEWORK_HEADERS_DIR)
  get_filename_component(QT_WIDGETS_FRAMEWORK_HEADERS_ROOT "${QT_WIDGETS_FRAMEWORK_HEADERS_DIR}" DIRECTORY)

  set_target_properties(obs-audio-recorder PROPERTIES
    PREFIX ""
    SUFFIX ".plugin"
    BUNDLE_EXTENSION "plugin"
    BUNDLE TRUE
    OUTPUT_NAME "obs-audio-recorder"
    MACOSX_RPATH TRUE
  )

  target_include_directories(obs-audio-recorder PRIVATE
    "${QT_CORE_FRAMEWORK_HEADERS_ROOT}"
    "${QT_CORE_FRAMEWORK_HEADERS_DIR}"
    "${QT_WIDGETS_FRAMEWORK_HEADERS_ROOT}"
    "${QT_WIDGETS_FRAMEWORK_HEADERS_DIR}"
  )

  target_compile_options(obs-audio-recorder PRIVATE
    "-F${QTBASE_PREFIX}/lib"
    -Wall
    -Wextra
    -Wpedantic
  )

  target_link_options(obs-audio-recorder PRIVATE
    "-undefined"
    "dynamic_lookup"
  )

  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
    "${CMAKE_CURRENT_BINARY_DIR}/obs-audio-recorder.plugin/Contents/Info.plist"
    @ONLY
  )

  set_source_files_properties(
    "${CMAKE_CURRENT_BINARY_DIR}/obs-audio-recorder.plugin/Contents/Info.plist"
    PROPERTIES
    GENERATED TRUE
  )
elseif(WIN32)
  find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

  set(OBS_LIBOBS_LIBRARY "" CACHE FILEPATH "Path to the libobs import library")
  set(OBS_FRONTEND_API_LIBRARY "" CACHE FILEPATH "Path to the obs-frontend-api import library")

  if(NOT OBS_LIBOBS_LIBRARY OR NOT EXISTS "${OBS_LIBOBS_LIBRARY}")
    message(FATAL_ERROR "Set OBS_LIBOBS_LIBRARY to the libobs import library from an OBS build or SDK.")
  endif()

  if(NOT OBS_FRONTEND_API_LIBRARY OR NOT EXISTS "${OBS_FRONTEND_API_LIBRARY}")
    message(FATAL_ERROR "Set OBS_FRONTEND_API_LIBRARY to the obs-frontend-api import library from an OBS build or SDK.")
  endif()

  set_target_properties(obs-audio-recorder PROPERTIES
    PREFIX ""
    OUTPUT_NAME "obs-audio-recorder"
  )

  target_link_libraries(obs-audio-recorder PRIVATE
    Qt6::Core
    Qt6::Widgets
    "${OBS_LIBOBS_LIBRARY}"
    "${OBS_FRONTEND_API_LIBRARY}"
  )

  target_compile_options(obs-audio-recorder PRIVATE
    /W4
    /permissive-
  )
else()
  message(FATAL_ERROR "This project currently has explicit build support for macOS and Windows only.")
endif()
