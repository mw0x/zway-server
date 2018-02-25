
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

#include "streambuffersender.h"
#include "server.h"
#include "logger.h"

// ============================================================ //

STREAM_BUFFER_SENDER StreamBufferSender::create(Server *server, const boost::shared_ptr<ClientSession> &session, uint32_t id)
{
    STREAM_BUFFER_SENDER sender = STREAM_BUFFER_SENDER(new StreamBufferSender(server, session, id));

    if (!sender->init()) {

        return nullptr;
    }

    return sender;
}

StreamBufferSender::StreamBufferSender(Server *server, const boost::shared_ptr<ClientSession> &session, uint32_t id)
    : Zway::BufferSender(id, Zway::Packet::Undefined, nullptr, nullptr),
      m_server(server),
      m_session(session)
{

}

bool StreamBufferSender::init()
{
    if (!StreamSender::init()) {

        return false;
    }

    getBuffer();

    return true;
}

bool StreamBufferSender::preparePacket(Zway::PACKET &pkt, size_t bytesToSend, size_t bytesSent)
{
    getBuffer();

    if (m_buffer) {

        return Zway::BufferSender::preparePacket(pkt, bytesToSend, bytesSent);
    }
    else {

        pkt.reset();
    }

    return true;
}

void StreamBufferSender::getBuffer()
{
    if (!m_buffer) {

        STREAM_BUFFER buffer = m_server->getStreamBuffer(m_id);

        if (buffer) {

            m_type = buffer->streamType();

            m_size = buffer->size();

            m_parts = buffer->streamParts();

            m_buffer = buffer;
        }
    }
}

boost::shared_ptr<ClientSession> StreamBufferSender::session()
{
    return m_session;
}

// ============================================================ //
