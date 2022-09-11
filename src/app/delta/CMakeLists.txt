
add_library(delta-service delta.proto)
target_link_libraries(delta-service
  PUBLIC
    protobuf::libprotobuf
    gRPC::grpc
    gRPC::grpc++
)

target_include_directories(delta-service
  PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROTOBUF_INCLUDE_PATH}
)

get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)

# compile the message types
protobuf_generate(TARGET delta-service LANGUAGE cpp)

# compile the GRPC services
protobuf_generate(
  TARGET
    delta-service
  LANGUAGE
    grpc
  GENERATE_EXTENSIONS
    .grpc.pb.h
    .grpc.pb.cc
  PLUGIN
    "protoc-gen-grpc=${grpc_cpp_plugin_location}"
)