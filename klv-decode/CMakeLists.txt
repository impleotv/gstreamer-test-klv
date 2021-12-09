cmake_minimum_required(VERSION 2.8)

set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH ON)

project(klvDecoder)


find_package(PkgConfig REQUIRED)
if ( NOT (PKGCONFIG_FOUND))
      message(FATAL_ERROR "Please Install PPkgConfig: CMake will Exit")
endif()
pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.8)
if ( NOT (GST_FOUND))
      message(FATAL_ERROR "Please Install Gstreamer Dev: CMake will Exit")
endif()
set(ENV{PKG_CONFIG_PATH})


include(FetchContent)

FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.7.3)

FetchContent_GetProperties(json)
if(NOT json_POPULATED)
  FetchContent_Populate(json)
  add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()



include_directories("${GST_INCLUDE_DIRS}")
link_libraries(${GST_LIBRARIES} ${CMAKE_DL_LIBS} )

ADD_EXECUTABLE(klvDecoder main.cpp)

TARGET_LINK_LIBRARIES (klvDecoder
    gstapp-1.0
    nlohmann_json::nlohmann_json
    -pthread
    )
