cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(buildspec)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

find_package(obs-frontend-api REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
set_target_properties(
  ${CMAKE_PROJECT_NAME}
  PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ../src/main.cpp)

target_compile_definitions(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    QT_NO_KEYWORDS
    QT_NO_VERSION_TAGGING
)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME "obs-audio-recorder" PREFIX "")

if(APPLE)
  set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES SUFFIX "")
  set(BUNDLE_ID "${MACOS_BUNDLEID}")

  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/Info.plist.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
    @ONLY
  )

  install(TARGETS ${CMAKE_PROJECT_NAME} LIBRARY DESTINATION "obs-audio-recorder.plugin/Contents/MacOS")

  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" DESTINATION "obs-audio-recorder.plugin/Contents" RENAME Info.plist)

  install(DIRECTORY ../data/ DESTINATION "obs-audio-recorder.plugin/Contents/Resources/data")
elseif(WIN32)
  install(TARGETS ${CMAKE_PROJECT_NAME} RUNTIME DESTINATION "obs-audio-recorder/bin/64bit" LIBRARY DESTINATION "obs-audio-recorder/bin/64bit")
  install(
    FILES $<TARGET_RUNTIME_DLLS:${CMAKE_PROJECT_NAME}>
    DESTINATION "obs-audio-recorder/bin/64bit"
  )

  install(DIRECTORY ../data/ DESTINATION "obs-audio-recorder/data")
else()
  message(FATAL_ERROR "Unsupported platform for CI build.")
endif()
