
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

#ifndef SESSION_H_
#define SESSION_H_

#include "thread.h"
#include "streambuffer.h"

#include "Zway/core/engine.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <queue>

// ============================================================ //

#define ZWAY_PORT 5557

#define HEARTBEAT_TIMEOUT 40000

// ============================================================ //

#define STATUS_DISCONNECTED         0
#define STATUS_CONNECTED            1
#define STATUS_LOGGEDIN             2

// ============================================================ //
// ClientSession
// ============================================================ //

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;

class DB;
class Server;

class ClientSession
    : public Zway::Engine,
      public boost::enable_shared_from_this<ClientSession>
{
public:

    typedef boost::shared_ptr<ClientSession> Pointer;

    static Pointer create(Server *server, boost::asio::io_service& io_service, boost::asio::ssl::context& context);

    void start();

    void close(bool shutdown = true, bool remove = true);


    uint32_t status();

    std::string remoteHost();

    uint32_t accountId();

    ssl_socket::lowest_layer_type& socket();


    static void ubjValToBson(const std::string &key, const Zway::UBJ::Value &val, mongo::BSONObjBuilder &ob);

    static mongo::BSONObj ubjToBson(const Zway::UBJ::Value &val);


    static Zway::UBJ::Value bsonToUbj(const mongo::BSONElement &ele);

    static Zway::UBJ::Object bsonObjToUbj(const mongo::BSONObj &obj);

    static Zway::UBJ::Array bsonArrToUbj(const mongo::BSONObj &arr);


protected:

    ClientSession(Server *server, boost::asio::io_service &io_service, boost::asio::ssl::context &context);


    void setStatus(uint32_t status);

    bool setConfig(const Zway::UBJ::Object &config);


    void onTimer(const boost::system::error_code &error);

    void resetTimer();

    void pauseTimer();


    void handleHandshake(const boost::system::error_code &error);

    void onPacketSent(
            const boost::system::error_code &error,
            size_t bytes_transferred,
            Zway::PACKET pkt);

    void onPacketRecv(Zway::PACKET pkt);

    void onPacketHeadRecv(
            const boost::system::error_code &error,
            size_t bytes_transferred,
            Zway::PACKET pkt);

    void onPacketBodyRecv(
            const boost::system::error_code &error,
            size_t bytes_transferred,
            size_t offset,
            Zway::PACKET pkt);


    bool sendPacket();

    bool recvPacket();

    void recvPacketBody(Zway::PACKET pkt);


    bool processRequests();


    bool processDispatchRequest(const Zway::UBJ::Object &head);

    bool processCreateAccount(const Zway::UBJ::Object &head);

    bool processLogin(const Zway::UBJ::Object &head);

    bool processLogout(const Zway::UBJ::Object &head);


    bool processConfigRequest(const Zway::UBJ::Object &head);

    bool processContactStatusRequest(const Zway::UBJ::Object &head);

    bool processFindContactRequest(const Zway::UBJ::Object &head);

    bool processAddContactRequest(const Zway::UBJ::Object &head);

    bool processCreateAddCode(const Zway::UBJ::Object &head);

    bool processAcceptContact(const Zway::UBJ::Object &head);

    bool processRejectContact(const Zway::UBJ::Object &head);

    bool processPushRequest(const Zway::UBJ::Object &head);


    void broadcastStatus(uint32_t status, bool check = true);

    uint32_t getContactStatus(uint32_t contactId);


    bool postPacket(Zway::PACKET pkt);


    bool addStreamSender(Zway::STREAM_SENDER sender);

    bool processIncomingRequest(const Zway::UBJ::Object &request);

    Zway::STREAM_RECEIVER createStreamReceiver(const Zway::Packet &pkt);

    Zway::UBJ::Object &config();

private:

    bool verifyPassword(const mongo::BSONObj &account, Zway::BUFFER password);

private:

    Server* m_server;

    ssl_socket m_socket;

    boost::asio::deadline_timer m_timer;

    ThreadSafe<uint32_t> m_status;

    std::string m_remoteHost;

    uint32_t m_accountId;

    uint32_t m_numPacketsSent;

    uint32_t m_numPacketsRecv;

    ThreadSafe<bool> m_sending;

    ThreadSafe<std::queue<Zway::PACKET>> m_packetQueue;

    ThreadSafe<std::map<uint32_t, Zway::UBJ::Object>> m_contacts;

    Zway::UBJ::Object m_config;

    friend class Server;
};

typedef ClientSession::Pointer CLIENT_SESSION;

typedef std::list<CLIENT_SESSION> CLIENT_SESSION_LIST;

typedef std::map<uint32_t, CLIENT_SESSION_LIST> CLIENT_SESSION_MAP;

// ============================================================ //

#endif /* SESSION_H_ */
