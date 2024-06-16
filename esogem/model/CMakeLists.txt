cmake_minimum_required(VERSION 3.29)

# to build:
#   mkdir build; cd build
#   CC=clang CXX=clang++ cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DSPM_PROTOBUF_PROVIDER=package ..
#   ninja -j4 esogem_model
# N.B. using high -j values is *very* memory-hungry for some of the deps

project(esogem VERSION 0.1)

# import gemma.cpp as a library
include(FetchContent)
FetchContent_Declare(sentencepiece GIT_REPOSITORY https://github.com/google/sentencepiece GIT_TAG 53de76561cfc149d3c01037f0595669ad32a5e7c)
FetchContent_Declare(gemma GIT_REPOSITORY https://github.com/google/gemma.cpp GIT_TAG 419dc34ed5af862a2337293e0042caae7043a9a3)
FetchContent_Declare(highway GIT_REPOSITORY https://github.com/google/highway.git GIT_TAG da250571a45826b21eebbddc1e50d0c1137dee5f)
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
target_link_libraries(esogem_model
  libgemma hwy hwy_contrib sentencepiece
  ${Protobuf_LIBRARIES})
target_include_directories(esogem_model
  PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${gemma_SOURCE_DIR} ${sentencepiece_SOURCE_DIR}
  ${Protobuf_INCLUDE_DIRS})
