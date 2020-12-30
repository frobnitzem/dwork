#find_package(Protobuf REQUIRED)

set(PROTO_INP TaskMsg.proto)
#set(PROTO_SRCS ${CMAKE_CURRENT_BINARY_DIR}/TaskMsg.pb.cc)
#set(PROTO_HDRS ${CMAKE_CURRENT_BINARY_DIR}/TaskMsg.pb.h)
#set(PROTO_SRCS ${PROTO_SRCS} PARENT_SCOPE)
#set(PROTO_HDRS ${PROTO_HDRS} PARENT_SCOPE)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_INP})
#add_custom_command(OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
#                   COMMAND ${Protobuf_PROTOC_EXECUTABLE} -I=${CMAKE_CURRENT_SOURCE_DIR}
#                      --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
#                      ${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_INP}
#                   MAIN_DEPENDENCY ${PROTO_INP}
#                   COMMENT "Generating protobuf"
#                  )

add_library(TaskMsg OBJECT ${PROTO_SRCS})
target_include_directories(TaskMsg PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
#target_link_libraries(TaskMsg PUBLIC dwork_compiler_flags protobuf::libprotobuf-lite)
target_link_libraries(TaskMsg PUBLIC protobuf::libprotobuf-lite)
set_target_properties(TaskMsg PROPERTIES
                      POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
                      )

install(FILES ${PROTO_HDRS} DESTINATION include)
