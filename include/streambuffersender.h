
// ============================================================ //
//
//   d88888D db   d8b   db  .d8b.  db    db
//   YP  d8' 88   I8I   88 d8' `8b `8b  d8'
//      d8'  88   I8I   88 88ooo88  `8bd8'
//     d8'   Y8   I8I   88 88~~~88    88
//    d8' db `8b d8'8b d8' 88   88    88
//   d88888P  `8b8' `8d8'  YP   YP    YP
//
//   open-source, cross-platform, crypto-messenger
//
//   Copyright (C) 2018 Marc Weiler
//
//   This program is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program. If not, see <http://www.gnu.org/licenses/>.
//
// ============================================================ //

#ifndef ZWAY_SERVER_STREAM_BUFFER_SENDER_H_
#define ZWAY_SERVER_STREAM_BUFFER_SENDER_H_

#include "Zway/core/buffersender.h"
#include "streambuffer.h"

// ============================================================ //

class Server;
class ClientSession;

class StreamBufferSender : public Zway::BufferSender
{
public:

    typedef std::shared_ptr<StreamBufferSender> Pointer;

    static Pointer create(Server *server, const boost::shared_ptr<ClientSession> &session, uint32_t id);

    boost::shared_ptr<ClientSession> session();

protected:

    StreamBufferSender(Server *server, const boost::shared_ptr<ClientSession> &session, uint32_t id);

    bool init();

    bool preparePacket(Zway::PACKET &pkt, size_t bytesToSend, size_t bytesSent);

    void getBuffer();

protected:

    Server *m_server;

    boost::shared_ptr<ClientSession> m_session;
};

typedef StreamBufferSender::Pointer STREAM_BUFFER_SENDER;

// ============================================================ //

#endif
