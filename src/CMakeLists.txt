cmake_minimum_required(VERSION 2.8)

project(gateway)

set(EXECUTABLE_NAME yworldgateway)
SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin)

set(SRC_GRP_CLIENT
  ${CMAKE_CURRENT_SOURCE_DIR}/client_connection.h
  ${CMAKE_CURRENT_SOURCE_DIR}/client_connection.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/client_accept.h
  ${CMAKE_CURRENT_SOURCE_DIR}/client_accept.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/connection_base.h
  ${CMAKE_CURRENT_SOURCE_DIR}/connection_base.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ikcp.h
  ${CMAKE_CURRENT_SOURCE_DIR}/ikcp.cpp
)

set(SRC_GRP_GATEWAY
  ${CMAKE_CURRENT_SOURCE_DIR}/gateway_accept.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gateway_accept.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/server_connection.h
  ${CMAKE_CURRENT_SOURCE_DIR}/server_connection.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gateway_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gateway_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gateway_profiler.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gateway_profiler.cpp
)

set(SRC_GRP_PB
  ${CMAKE_CURRENT_SOURCE_DIR}/../../gameserver/common/proto/proto_common.pb.h
  ${CMAKE_CURRENT_SOURCE_DIR}/../../gameserver/common/proto/proto_common.pb.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../gameserver/common/proto/proto_server.pb.h
  ${CMAKE_CURRENT_SOURCE_DIR}/../../gameserver/common/proto/proto_server.pb.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../gameserver/common/proto/proto_client.pb.h
  ${CMAKE_CURRENT_SOURCE_DIR}/../../gameserver/common/proto/proto_client.pb.cc
)

set(SRC_GRP_RPC
  queueserver_api.h
  queueserver_services.h
  queueserver_services.cpp
)

set(EXECUTABLE_SRCS
  main.cpp
  dump_generator.h
  dump_generator.cpp
  ${SRC_GRP_CLIENT}
  ${SRC_GRP_GATEWAY}
  ${SRC_GRP_PB}
  ${SRC_GRP_QUEUE}
  ${SRC_GRP_RPC}
)

source_group("Rpc"
  FILES
    ${SRC_GRP_RPC}
)

source_group("Client"
  FILES
    ${SRC_GRP_CLIENT}
)

source_group("Gateway"
  FILES
    ${SRC_GRP_GATEWAY}
)

source_group("Pb"
  FILES
    ${SRC_GRP_PB}
)

source_group("Queue"
  FILES
    ${SRC_GRP_QUEUE}
)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../base_util
  ${CMAKE_CURRENT_SOURCE_DIR}/../../gameserver/common
  ${YYRPC_INCLUDE_DIRS}
  ${LIBUV_INCLUDE_DIR}
  ${SPDLOG_INCLUDE_DIRS}
  ${PROTOBUF_INCLUDE_DIR}
  ${BREAKPAD_INCLUDE_DIR}
)

add_executable(${EXECUTABLE_NAME}
  ${EXECUTABLE_SRCS}
)

target_link_libraries(${EXECUTABLE_NAME}
  ${GLOG_LIBRARIES}
  ${LIBUV_LIBRARIES}
  ${PROTOBUF_LIBRARIES}
  ${CURL_LIBRARIES}
  ${BREAKPAD_LIBRARIES}
  ${YYRPC_LIBRARIES}
)

if(WIN32)

target_link_libraries(${EXECUTABLE_NAME}
  advapi32
  iphlpapi
  shell32
  user32
)

elseif(UNIX)

target_link_libraries(${EXECUTABLE_NAME}
  m 
  dl
)

endif()

if(UNIX)
  set(EXECUTABLE_LINK_FLAGS "-pthread ${EXECUTABLE_LINK_FLAGS} -rdynamic")
endif()

if(APPLE)
  set(EXECUTABLE_LINK_FLAGS "-framework Carbon ${EXECUTABLE_LINK_FLAGS}")
endif()

set_target_properties(${EXECUTABLE_NAME} PROPERTIES LINK_FLAGS
  "${EXECUTABLE_LINK_FLAGS}"
)
