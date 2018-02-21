
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
//   Copyright (C) 2012-2016  Marc Weiler
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
