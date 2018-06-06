cmake_minimum_required(VERSION 3.10)
project(MTServer)

set(CMAKE_CXX_STANDARD 14)

add_executable(Server server.cpp ZConfig.h Server.h)
add_executable(Client client.cpp Client.cpp Client.h ZConfig.h ObjectPool.h)

find_library(PTHREAD pthread)
find_library(ZMQ zmq)

SET_TARGET_PROPERTIES(Client
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
        )

TARGET_LINK_LIBRARIES(Client
        ${ZMQ}
        ${PTHREAD}
        )

SET_TARGET_PROPERTIES(Server
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
        )

TARGET_LINK_LIBRARIES(Server
        ${ZMQ}
        ${PTHREAD}
        )