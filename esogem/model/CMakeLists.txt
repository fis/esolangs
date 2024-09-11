cmake_minimum_required(VERSION 3.25)

# to build:
#   mkdir build; cd build
#   CC=clang CXX=clang++ cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DSPM_PROTOBUF_PROVIDER=package ..
#   ninja -j4 esogem_model
# N.B. using high -j values is *very* memory-hungry for some of the deps

project(esogem VERSION 0.1)

# import gemma.cpp as a library
include(FetchContent)
FetchContent_Declare(sentencepiece GIT_REPOSITORY https://github.com/google/sentencepiece GIT_TAG 53de76561cfc149d3c01037f0595669ad32a5e7c)
FetchContent_Declare(gemma GIT_REPOSITORY https://github.com/google/gemma.cpp GIT_TAG 1982a6ba00a2f94fdf7178dfa0e8946acaea7c7b)
FetchContent_Declare(highway GIT_REPOSITORY https://github.com/google/highway.git GIT_TAG 457c891775a7397bdb0376bb1031e6e027af1c48)
FetchContent_MakeAvailable(sentencepiece gemma highway)
FetchContent_GetProperties(gemma)
FetchContent_GetProperties(sentencepiece)

# compile protos
find_package(Protobuf REQUIRED)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS model.proto)

# model host executable
add_executable(esogem_model
  main.cc
  ${PROTO_SRCS} ${PROTO_HDRS}
)
set_property(TARGET esogem_model PROPERTY CXX_STANDARD 20)
target_link_libraries(esogem_model
  libgemma hwy hwy_contrib sentencepiece
  ${Protobuf_LIBRARIES})
target_include_directories(esogem_model
  PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${gemma_SOURCE_DIR} ${sentencepiece_SOURCE_DIR}
  ${Protobuf_INCLUDE_DIRS})
