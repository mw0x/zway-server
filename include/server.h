
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

#ifndef SERVER_H_
#define SERVER_H_

#include "db.h"
#include "session.h"
#include "fcmsender.h"
#include "streambuffersender.h"

#include <boost/thread.hpp>

// ============================================================ //
// Server
// ============================================================ //

class Server
{
    public:

        Server(boost::shared_ptr<boost::asio::io_service> io_service);

        bool start(const std::string &workingDir, const std::string& address, uint32_t port);

        void close();

        bool pause();

        bool resume();


        void appendSession(CLIENT_SESSION session);

        void removeSession(CLIENT_SESSION session);

        void removeSessions();

        size_t getSessionCount();

        CLIENT_SESSION_LIST getSessions(uint32_t userId);


        void processUserRequests(uint32_t userId);


        bool addStreamBuffer(STREAM_BUFFER buffer);

        bool removeStreamBuffer(uint32_t id);

        STREAM_BUFFER getStreamBuffer(uint32_t id);


        void addStreamBufferSender(STREAM_BUFFER_SENDER sender);


        size_t numStreamBuffers();


        void info();

        bool paused() const;

        boost::shared_ptr<boost::asio::io_service> io_service();

    private:

        void onTimer(const boost::system::error_code &error);

        std::string uptimeStr();

        void accept();

        void handleSession(
                const boost::system::error_code& error,
                CLIENT_SESSION session);
    private:

        bool m_paused;

        uint32_t m_numSessions;

        boost::shared_ptr<boost::asio::io_service> m_io_service;

        boost::asio::deadline_timer m_timer;

        boost::posix_time::ptime m_startTime;

        boost::asio::ssl::context m_context;

        boost::asio::ip::tcp::acceptor m_acceptor;

        boost::asio::ip::tcp::socket::endpoint_type m_endpoint;

        ThreadSafe<CLIENT_SESSION_MAP> m_sessions;

        // TODO cleanup mechanism for buffers

        ThreadSafe<std::map<uint32_t, STREAM_BUFFER>> m_buffers;

        ThreadSafe<std::list<STREAM_BUFFER_SENDER>> m_senders;
};

// ============================================================ //

#endif /* SERVER_H_ */
