# CMakeList.txt : CMake project for pelib, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("pelib")

INCLUDE_DIRECTORIES(
  include
  ${CMAKE_SOURCE_DIR}/include
  )

FILE(GLOB pelib_src ${CMAKE_SOURCE_DIR}/pelib/*.cpp)

set(CMAKE_BINARY_DIR "/out")

add_library(pelib STATIC ${pelib_src})

