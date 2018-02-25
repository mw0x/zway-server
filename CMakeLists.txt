
## ============================================================ ##
##
##   d88888D db   d8b   db  .d8b.  db    db
##   YP  d8' 88   I8I   88 d8' `8b `8b  d8'
##      d8'  88   I8I   88 88ooo88  `8bd8'
##     d8'   Y8   I8I   88 88~~~88    88
##    d8' db `8b d8'8b d8' 88   88    88
##   d88888P  `8b8' `8d8'  YP   YP    YP
##
##   open-source, cross-platform, crypto-messenger
##
##   Copyright (C) 2018 Marc Weiler
##
##   This library is free software; you can redistribute it and/or
##   modify it under the terms of the GNU Lesser General Public
##   License as published by the Free Software Foundation; either
##   version 2.1 of the License, or (at your option) any later version.
##
##   This library is distributed in the hope that it will be useful,
##   but WITHOUT ANY WARRANTY; without even the implied warranty of
##   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
##   Lesser General Public License for more details.
##
## ============================================================ ##

project(ZwayServer)

cmake_minimum_required(VERSION 2.8)

set(CMAKE_VERBOSE_MAKEFILE ON)

set(BOOST_PATH "" CACHE FILEPATH "")

set(MONGODB_PATH "" CACHE FILEPATH "")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wno-deprecated-declarations")

set(BOOST_ROOT ${BOOST_PATH})

find_package(Boost 1.53.0 COMPONENTS regex chrono thread system program_options log_setup log)

add_definitions(-DBOOST_LOG_DYN_LINK)

add_definitions(-DZWAY_SERVER)

find_package(OpenSSL)

include_directories(
    ${OPENSSL_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${MONGODB_PATH}/include
    ../zway-lib/include
    include
)

link_directories(
    ${MONGODB_PATH}/lib
)

set(zway_SRCS

    ../zway-lib/src/core/ubj/dumper.cpp
    ../zway-lib/src/core/ubj/reader.cpp
    ../zway-lib/src/core/ubj/ubjr.c
    ../zway-lib/src/core/ubj/ubjw.c
    ../zway-lib/src/core/ubj/value.cpp
    ../zway-lib/src/core/ubj/writer.cpp

    ../zway-lib/src/core/buffer.cpp
    ../zway-lib/src/core/bufferreceiver.cpp
    ../zway-lib/src/core/buffersender.cpp
    ../zway-lib/src/core/engine.cpp
    ../zway-lib/src/core/packet.cpp
    ../zway-lib/src/core/request.cpp
    ../zway-lib/src/core/streamreceiver.cpp
    ../zway-lib/src/core/streamsender.cpp
    ../zway-lib/src/core/ubjreceiver.cpp
    ../zway-lib/src/core/ubjsender.cpp

    ../zway-lib/src/event/event.cpp
    ../zway-lib/src/request/requestevent.cpp

)

set(server_SRCS

    src/db.cpp
    src/fcmsender.cpp
    src/logger.cpp
    src/main.cpp
    src/server.cpp
    src/session.cpp
    src/streambuffer.cpp
    src/streambuffersender.cpp
    src/request/addcontact.cpp
    src/request/acceptcontact.cpp
    src/request/rejectcontact.cpp
    src/request/pushrequest.cpp
    src/request/dispatch.cpp

)

add_executable (server ${zway_SRCS} ${server_SRCS})

target_link_libraries(server
    ${OPENSSL_LIBRARIES}
    ${Boost_LIBRARIES}
    mongoclient
    pthread
    curl
)

#install(TARGETS server
#    RUNTIME DESTINATION bin
#    LIBRARY DESTINATION lib
#)
