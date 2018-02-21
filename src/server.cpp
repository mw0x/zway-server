
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

#include "logger.h"
#include "server.h"

#include <boost/date_time/posix_time/posix_time.hpp>

// ============================================================ //

Server::Server(boost::shared_ptr<boost::asio::io_service> io_service)
    : m_paused(false),
      m_numSessions(0),
      m_io_service(io_service),
      m_timer(*io_service),
      m_context(*io_service, boost::asio::ssl::context::tlsv12_server),
      m_acceptor(*io_service)
{
}

// ============================================================ //

bool Server::start(const std::string &workingDir, const std::string& address, uint32_t port)
{
    // init database connection

    // TODO: we need a pool of db clients, due to the fact
    // one client instance should only be used by
    // one single thread (semaphore?)

    if (!DB::startup("127.0.0.1", 25)) {

        return false;
    }

    // init acceptor socket

    try {

        m_context.set_options(
                boost::asio::ssl::context::default_workarounds|
                boost::asio::ssl::context::no_sslv2|
                boost::asio::ssl::context::no_sslv3|
                boost::asio::ssl::context::single_dh_use);

        std::string certsDir = workingDir + "/certs";

        m_context.use_certificate_chain_file(certsDir + "/x509-server.pem");

        m_context.use_private_key_file(certsDir + "/x509-server-key.pem", boost::asio::ssl::context::pem);

        // TODO set dh params file here IMPORTANT

        using namespace boost::asio::ip;

        tcp::resolver resolver(m_acceptor.get_io_service());

        tcp::resolver::query query(tcp::v4(), address, boost::lexical_cast<std::string>(port));

        tcp::resolver::iterator iterator = resolver.resolve(query);

        m_endpoint = *iterator;

        m_acceptor.open(tcp::v4());

        m_acceptor.set_option(tcp::acceptor::reuse_address(true));

        m_acceptor.bind(m_endpoint);

        m_acceptor.listen();
    }
    catch (std::exception& e) {

        LOG_ERROR << e.what();

        return false;
    }

    m_timer.expires_from_now(boost::posix_time::milliseconds(2000));

    m_timer.async_wait(
                boost::bind(
                    &Server::onTimer,
                    this,
                    boost::asio::placeholders::error));

    // start serving clients

    m_startTime = boost::posix_time::second_clock::local_time();

    accept();

    LOG_INFO << "server started";

    return true;
}

// ============================================================ //

void Server::close()
{
    boost::system::error_code ec;

    m_timer.cancel(ec);

    // close acceptor socket

    m_acceptor.close(ec);

    // close remaining sessions

    removeSessions();

    // close db

    DB::cleanup();

    // some final words

    /*
    LOG_INFO("server finished\n"
          "Duration: %s\n"
          "Sessions: %u",
          getUptimeStr().c_str(),
          m_numSessions);
          */
}

// ============================================================ //

bool Server::pause()
{
    if (m_paused) {
        return false;
    }

    try {

        m_acceptor.close();

        m_paused = true;
    }
    catch (std::exception& e) {

        LOG_ERROR << e.what();

        return false;
    }

    return true;
}

// ============================================================ //

bool Server::resume()
{
    if (!m_paused) {
        return false;
    }

    try {

        m_acceptor.open(boost::asio::ip::tcp::v4());

        //m_acceptor.set_option(tcp::acceptor::reuse_address(true));

        m_acceptor.bind(m_endpoint);

        m_acceptor.listen();

        accept();

        m_paused = false;
    }
    catch (std::exception& e) {

        LOG_ERROR << e.what();

        return false;
    }

    return true;
}

// ============================================================ //

void Server::appendSession(CLIENT_SESSION session)
{
	boost::mutex::scoped_lock l(m_sessions);

    ((*m_sessions)[session->accountId()]).push_back(session);
}

// ============================================================ //

void Server::removeSession(CLIENT_SESSION session)
{
    boost::mutex::scoped_lock l(m_sessions);

    uint32_t accountId = session->accountId();

    if (m_sessions->find(accountId) != m_sessions->end()) {

        CLIENT_SESSION_LIST& sm = (*m_sessions)[accountId];

        sm.remove(session);

        if (sm.empty()) {

            m_sessions->erase(accountId);
        }
    }
}

// ============================================================ //

void Server::removeSessions()
{
    boost::mutex::scoped_lock l(m_sessions);

    if (!m_sessions->empty()) {

        LOG_INFO << "Disconnecting " << m_sessions->size() << " users";

        for (auto &it : (*m_sessions)) {

            for (auto &s : it.second) {

                // TODO shutdown connection properly/gracefully

                s->close(false, false);
            }
        }

        m_sessions->clear();
    }
}

// ============================================================ //

size_t Server::getSessionCount()
{
	boost::mutex::scoped_lock l(m_sessions);

    return m_sessions->size();
}

// ============================================================ //

CLIENT_SESSION_LIST Server::getSessions(uint32_t userId)
{
	boost::mutex::scoped_lock l(m_sessions);

    CLIENT_SESSION_LIST res;

    if (m_sessions->find(userId) != m_sessions->end()) {

        res = (*m_sessions)[userId];
    }

    return res;
}

// ============================================================ //

void Server::processUserRequests(uint32_t userId)
{
    CLIENT_SESSION_LIST sessions = getSessions(userId);

    if (!sessions.empty()) {

        for (auto &s : sessions) {

            s->processRequests();
        }
    }
    else {

        std::string fcmToken = DB::getFcmToken(userId);

        if (!fcmToken.empty()) {

            uint32_t numContactRequests = DB::numContactRequests(userId);

            if (numContactRequests > 0) {

                FcmSender::sendMessage(fcmToken, 1000, numContactRequests);
            }

            uint32_t numMessages = DB::numPushRequests(userId);

            if (numMessages > 0) {

                FcmSender::sendMessage(fcmToken, 2000, numMessages);
            }
        }
    }
}

// ============================================================ //

bool Server::addStreamBuffer(STREAM_BUFFER buffer)
{
    boost::mutex::scoped_lock locker(m_buffers);

    if (m_buffers->find(buffer->streamId()) != m_buffers->end()) {

        return false;
    }

    (*m_buffers)[buffer->streamId()] = buffer;

    return true;
}

// ============================================================ //

bool Server::removeStreamBuffer(uint32_t id)
{
    boost::mutex::scoped_lock locker(m_buffers);

    if (m_buffers->find(id) == m_buffers->end()) {

        return false;
    }

    m_buffers->erase(id);

    return true;
}

// ============================================================ //

STREAM_BUFFER Server::getStreamBuffer(uint32_t id)
{
    boost::mutex::scoped_lock locker(m_buffers);

    if (m_buffers->find(id) != m_buffers->end()) {

        return (*m_buffers)[id];
    }
    else {

        std::stringstream ss;

        ss << "tmp/" << id;

        STREAM_BUFFER buf = StreamBuffer::open(ss.str(), id);

        if (buf) {

            (*m_buffers)[id] = buf;

            return buf;
        }
    }

    return nullptr;
}

// ============================================================ //

void Server::addStreamBufferSender(STREAM_BUFFER_SENDER sender)
{
    boost::mutex::scoped_lock locker(m_senders);

    m_senders->push_back(sender);
}

// ============================================================ //

size_t Server::numStreamBuffers()
{
    boost::mutex::scoped_lock lock(m_buffers);

    return m_buffers->size();
}

// ============================================================ //

void Server::info()
{
    std::string sessionList;

    boost::mutex::scoped_lock locker(m_sessions);

    for (auto &it : (*m_sessions)) {

        for (auto &s : it.second) {

            std::stringstream ss;

            mongo::BSONObj account;

            if (DB::getAccount(BSON("id" << s->accountId()), BSON("name" << 1), account)) {

                ss << s->remoteHost() << "\t" << account["name"].str() << "\n";
            }
            else {

                ss << s->remoteHost() << "\t?\n";
            }

            sessionList += ss.str();
        }
    }

    LOG_INFO <<
                "Status info:\n" <<
                "Duration: " << uptimeStr() << "\n" <<
                "Stream buffers: " << numStreamBuffers() << "\n" <<
                "Sessions: " << m_sessions->size() << "\n" <<
                sessionList;
}

// ============================================================ //

bool Server::paused() const
{
    return m_paused;
}

// ============================================================ //

boost::shared_ptr<boost::asio::io_service> Server::io_service()
{
    return m_io_service;
}

// ============================================================ //

void Server::onTimer(const boost::system::error_code &error)
{
    if (!error) {

        {
            boost::mutex::scoped_lock locker(m_senders);

            std::list<STREAM_BUFFER_SENDER> remove;

            for (STREAM_BUFFER_SENDER &sender : *m_senders) {

                if (sender->status() == StreamBufferSender::Completed) {

                    remove.push_back(sender);
                }
                else {

                    sender->session()->sendPacket();
                }
            }

            for (STREAM_BUFFER_SENDER &sender : remove) {

                m_senders->remove(sender);
            }
        }


        {
            boost::mutex::scoped_lock locker(m_buffers);

            uint64_t t = time(nullptr);

            std::list<STREAM_BUFFER> remove;

            for (auto &it : *m_buffers) {

                STREAM_BUFFER &buffer = it.second;

                if (t - buffer->lastActivity() > 60) {

                    remove.push_back(buffer);
                }
            }

            for (STREAM_BUFFER &buffer : remove) {

                m_buffers->erase(buffer->streamId());
            }
        }


        m_timer.expires_from_now(boost::posix_time::milliseconds(2000));

        m_timer.async_wait(
                    boost::bind(
                        &Server::onTimer,
                        this,
                        boost::asio::placeholders::error));
    }
}

// ============================================================ //

std::string Server::uptimeStr()
{
    // calculate up-time

    boost::posix_time::time_duration upTime =
            boost::posix_time::second_clock::local_time() -
            m_startTime;

    std::stringstream ss;
    if (upTime.hours() / 24 > 0) ss << upTime.hours() / 24 << " day(s) ";
    if (upTime.hours() % 24 > 0) ss << upTime.hours() % 24 << " hour(s) ";
    if (upTime.minutes() > 0) ss << upTime.minutes() << " minute(s) ";
    if (upTime.seconds() > 0) ss << upTime.seconds() << " second(s) ";

    return ss.str();
}

// ============================================================ //

void Server::accept()
{
    CLIENT_SESSION session =
            ClientSession::create(
                    this,
                    m_acceptor.get_io_service(),
                    m_context);

    m_acceptor.async_accept(
            session->socket(),
            boost::bind(&Server::handleSession,
                    this,
                    boost::asio::placeholders::error,
                    session));
}

// ============================================================ //

void Server::handleSession(
        const boost::system::error_code& error,
        CLIENT_SESSION session)
{
    if (!error) {

        session->start();

        m_numSessions++;

        accept();
    }
    else {

        if (error == boost::asio::error::operation_aborted) {

            return;
        }

        LOG_ERROR << "handleSession: " << error.message();
    }
}

// ============================================================ //
