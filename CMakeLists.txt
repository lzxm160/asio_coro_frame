cmake_minimum_required(VERSION 2.8)


project(asio_coro_frame C CXX)

enable_testing()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

set(CXX_FLAGS
 # -DVALGRIND
 # -DMUDUO_STD_STRING
 -DCHECK_PTHREAD_RETURN_VALUE
 -D_FILE_OFFSET_BITS=64
 -DBOOST_COROUTINES_NO_DEPRECATION_WARNING
 -Wall
 -Wextra
 #-Werror
 #-Wconversion
 -Wno-unused-parameter
# -Wold-style-cast
 -Woverloaded-virtual
 -Wpointer-arith
 -Wshadow
 -Wwrite-strings
 -march=native
 # -MMD
 -std=c++14
 -rdynamic
 )
if(CMAKE_BUILD_BITS EQUAL 32)
  list(APPEND CXX_FLAGS "-m32")
endif()
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

#set(CMAKE_CXX_COMPILER "/usr/bin/g++")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -pg")
#set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -finline-limit=1000 -DNDEBUG")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
LINK_DIRECTORIES(${PROJECT_BINARY_DIR}/lib)
find_package(Boost REQUIRED)
find_package(Protobuf)
find_package(CURL)
find_package(ZLIB)
find_path(CARES_INCLUDE_DIR ares.h)
find_library(CARES_LIBRARY NAMES cares)
find_path(MHD_INCLUDE_DIR microhttpd.h)
find_library(MHD_LIBRARY NAMES microhttpd)
find_library(BOOSTTEST_LIBRARY NAMES boost_unit_test_framework)
find_library(BOOSTPO_LIBRARY NAMES boost_program_options)
find_path(TCMALLOC_INCLUDE_DIR gperftools/heap-profiler.h)
find_library(TCMALLOC_LIBRARY NAMES tcmalloc_and_profiler)
find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
find_library(HIREDIS_LIBRARY NAMES hiredis)
find_path(GD_INCLUDE_DIR gd.h)
find_library(GD_LIBRARY NAMES gd)
find_program(THRIFT_COMPILER thrift)
find_path(THRIFT_INCLUDE_DIR thrift)
find_library(THRIFT_LIBRARY NAMES thrift)

if(CARES_INCLUDE_DIR AND CARES_LIBRARY)
  message(STATUS "found cares")
endif()
if(CURL_FOUND)
  message(STATUS "found curl")
endif()
if(PROTOBUF_FOUND)
  message(STATUS "found protobuf")
endif()
if(TCMALLOC_INCLUDE_DIR AND TCMALLOC_LIBRARY)
  message(STATUS "found tcmalloc")
endif()
if(ZLIB_FOUND)
  message(STATUS "found zlib")
endif()
if(HIREDIS_INCLUDE_DIR AND HIREDIS_LIBRARY)
  message(STATUS "found hiredis")
endif()
if(GD_INCLUDE_DIR AND GD_LIBRARY)
  message(STATUS "found gd")
endif()
if(THRIFT_COMPILER AND THRIFT_INCLUDE_DIR AND THRIFT_LIBRARY)
  message(STATUS "found thrift")
endif()

include_directories(${Boost_INCLUDE_DIRS})

include_directories(${PROJECT_SOURCE_DIR})

string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
message(STATUS "CXX_FLAGS = " ${CMAKE_CXX_FLAGS} " " ${CMAKE_CXX_FLAGS_${BUILD_TYPE}})

if (NOT NO_SUB)
    option(ENABLE_BOOST_CONTEXT "use boost coroutine" ON)
    #add_subdirectory("libgo")
endif()

add_subdirectory(src)
configure_file(${PROJECT_SOURCE_DIR}/src/config.ini  ${PROJECT_BINARY_DIR}/bin/config.ini COPYONLY)
#configure_file(${PROJECT_SOURCE_DIR}/lib/libredispp.a  ${PROJECT_BINARY_DIR}/lib/libredispp.a COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/src/mexic_price.xlsx ${PROJECT_BINARY_DIR}/bin/mexic_price.xlsx COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/web/index.html ${PROJECT_BINARY_DIR}/bin/web/index.html COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/src/tools/work_attendance/before.xlsx ${PROJECT_BINARY_DIR}/bin/before.xlsx COPYONLY)

configure_file(${PROJECT_SOURCE_DIR}/src/fastcgi/wkhtmltox/libwkhtmltox.so ${PROJECT_BINARY_DIR}/lib/libwkhtmltox.so COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/src/fastcgi/wkhtmltox/test.html ${PROJECT_BINARY_DIR}/bin/test.html COPYONLY)

file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/bin/logs)

#ADD_DEFINITIONS()