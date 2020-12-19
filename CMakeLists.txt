cmake_minimum_required(VERSION 3.17)

#####################################################################
# ATTENTION: Replace languages with your own list.                  #
project(dwork VERSION 1.0 LANGUAGES CXX C)
#####################################################################

# Add the cmake folder so the FindSphinx module is found
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Global Options
add_library(dwork_compiler_flags INTERFACE)
#####################################################################
# ATTENTION: add all relevant compiler flags here as cmake features #
# see https://cmake.org/cmake/help/latest/manual/cmake-compile-features.7.html#manual:cmake-compile-features(7)
# note: INTERFACE is correct here, since 'dwork_compiler_flags' is a #
# "target", so things that depend on it see only its "public"       #
# and "interface" properties.                                       #
target_compile_features(dwork_compiler_flags INTERFACE cxx_std_17)
#####################################################################

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option(BUILD_TESTS "Build the tests accompanying this library." ON)
configure_file(include/dwork.hh.in include/dwork.hh)


find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(cppzmq REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(TKRZW tkrzw REQUIRED IMPORTED_TARGET)

# can't use this, since it doesn't put the generated include file in include/
#protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS proto/TaskMsg.proto)
set(PROTO_SRCS proto/TaskMsg.pb.cc)
set(PROTO_HDRS include/TaskMsg.pb.h)

##########  TARGETS  ################################################
add_library(dwork src/taskDB.cc src/taskHeap.cc src/proto.cc src/dag.cc ${PROTO_SRCS})
add_executable(manager src/manager.cc)
add_executable(dhub   src/dhub.cc)
add_executable(dquery src/dquery.cc)
#add_subdirectory ("docs")
#####################################################################

# library code properties

target_link_libraries(dwork PUBLIC dwork_compiler_flags protobuf::libprotobuf-lite cppzmq PkgConfig::TKRZW Threads::Threads)
target_include_directories(dwork
                           PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
                            $<INSTALL_INTERFACE:include>
                           )
# lib needs PIC when BUILD_SHARED_LIBS=ON
set_target_properties(dwork PROPERTIES
                      POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
                      )

target_link_libraries(manager dwork)
target_link_libraries(dhub   dwork)
target_link_libraries(dquery dwork)

if(BUILD_TESTS)
  add_subdirectory(tests)
endif()

#####################################################################
# ATTENTION: add all library targets you want to install here:      #
set(installable_libs dwork dwork_compiler_flags)
#####################################################################

# Attach these libraries to the dworkTargets list of exported libs.
install(TARGETS ${installable_libs}
        DESTINATION lib
        EXPORT dworkTargets)

#####################################################################
# ATTENTION: add all your library's installed headers to this list  #
set(DWORK_HEADERS
        "${PROJECT_BINARY_DIR}/include/dwork.hh"
        "${PROTO_HDRS}"
        include/dag.hh
        include/dag_thread.h
        include/proto.hh
        include/taskDB.hh
        include/taskHeap.hh
        include/event.hh
    )
#####################################################################
install(FILES ${DWORK_HEADERS} DESTINATION include)

# Note: we choose the following location for cmake dependency info:
# <prefix>/lib/cmake/dwork/
# install the targets to export
install(EXPORT dworkTargets
  FILE dworkTargets.cmake
  NAMESPACE dwork::
  DESTINATION lib/cmake/dwork
)

# Create a config helper so others can call find_package(dwork::dwork)
include(CMakePackageConfigHelpers)
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/dworkConfig.cmake"
  INSTALL_DESTINATION "lib/cmake/dwork"
  NO_SET_AND_CHECK_MACRO
  )
# generate the version file for the config file
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/dworkConfigVersion.cmake"
  VERSION "${dwork_VERSION_MAJOR}.${dwork_VERSION_MINOR}"
  COMPATIBILITY AnyNewerVersion
)
# install the configuration file
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/dworkConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/dworkConfigVersion.cmake
  DESTINATION lib/cmake/dwork
  )
# allow this project to be used from the build directory (before install)
#export(EXPORT dworkTargets
#  FILE "${CMAKE_CURRENT_BINARY_DIR}/dworkTargets.cmake"
#)
