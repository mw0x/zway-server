
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
#include "session.h"
#include "streambuffersender.h"
#include "request/addcontact.h"
#include "request/rejectcontact.h"
#include "request/acceptcontact.h"
#include "request/pushrequest.h"
#include "request/dispatch.h"

#include "Zway/core/ubjreceiver.h"

#include <sstream>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/hex.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>

#include <openssl/rand.h>

using namespace mongo;

// ============================================================ //
// ClientSession
// ============================================================ //

CLIENT_SESSION ClientSession::create(Server *server, boost::asio::io_service &io_service, boost::asio::ssl::context &context)
{
    return CLIENT_SESSION(new ClientSession(server, io_service, context));
}

// ============================================================ //

ClientSession::ClientSession(Server *server, boost::asio::io_service &io_service, boost::asio::ssl::context &context)
    : Zway::Engine(),
      m_server(server),
      m_socket(io_service, context),
      m_timer(io_service),
      m_status(0),
      m_accountId(0),
      m_numPacketsSent(0),
      m_numPacketsRecv(0),
      m_sending(false)
{

}

// ============================================================ //

void ClientSession::setStatus(uint32_t status)
{
    boost::mutex::scoped_lock locker(m_status);

    m_status = status;
}

// ============================================================ //

bool ClientSession::setConfig(const Zway::UBJ::Object &config)
{
    if (config.hasField("contacts")) {

        boost::mutex::scoped_lock locker(m_contacts);

        m_contacts->clear();

        for (auto &it : config["contacts"].toArray()) {

            (*m_contacts)[it["contactId"].toInt()] = it;
        }
    }


    bool newStatus = config["notifyStatus"].toBool();

    bool curStatus = m_config["notifyStatus"].toBool();


    if (config.hasField("fcmToken")) {

        DB::setFcmToken(accountId(), config["fcmToken"].toStr());
    }


    m_config = config;


    if(newStatus != curStatus) {

        broadcastStatus(newStatus ? 1 : 0, false);
    }


    return true;
}

// ============================================================ //

void ClientSession::start()
{
	// create temporary id for this session,
    // until it's authenticated

    RAND_pseudo_bytes((uint8_t*)&m_accountId, sizeof(m_accountId));

	m_server->appendSession(shared_from_this());

    // start handshake

    m_socket.async_handshake(
            boost::asio::ssl::stream_base::server,
            boost::bind(
                    &ClientSession::handleHandshake,
                    shared_from_this(),
                    boost::asio::placeholders::error));
}

// ============================================================ //

void ClientSession::close(bool shutdown, bool remove)
{
    // TODO: we need to cancel pending receivers here or continue feature

    boost::system::error_code ec;

    m_timer.cancel(ec);

    // cancel pending socket requests

    m_socket.lowest_layer().cancel(ec);

    if (ec) {

        LOG_ERROR << "socket cancel: " << ec.message();
    }

    // shutdown socket

    if (shutdown) {

        m_socket.shutdown(ec);

        if (ec) {

            LOG_ERROR << "socket shutdown: " << ec.message();
        }
    }

    // close socket

    m_socket.lowest_layer().close(ec);

    if (ec) {

        LOG_ERROR << "close: " << ec.message();
    }

    // update status

    setStatus(STATUS_DISCONNECTED);

    // remove session

    if (remove) {

        m_server->removeSession(shared_from_this());
    }

    LOG_INFO << remoteHost() << " > session finished";
}

// ============================================================ //

void ClientSession::onTimer(const boost::system::error_code &error)
{
    if (!error) {

        //LOG_ERROR << remoteHost() << " > no heartbeat in " << HEARTBEAT_TIMEOUT << " ms";

        //close(false, false);
    }
}

// ============================================================ //

void ClientSession::resetTimer()
{
    // reset timer

    m_timer.expires_from_now(boost::posix_time::milliseconds(HEARTBEAT_TIMEOUT));

    m_timer.async_wait(
                boost::bind(
                    &ClientSession::onTimer,
                    shared_from_this(),
                    boost::asio::placeholders::error));
}

// ============================================================ //

void ClientSession::pauseTimer()
{
    m_timer.cancel();
}

// ============================================================ //

void ClientSession::handleHandshake(const boost::system::error_code &error)
{
    if (!error) {

        // set status

        setStatus(STATUS_CONNECTED);

        // build remote host address string

        std::stringstream s;
        s << socket().remote_endpoint().address().to_string() << ":"
          << socket().remote_endpoint().port();

        m_remoteHost = s.str();

        // start heartbeat timer

        resetTimer();

        LOG_INFO << remoteHost() << " > session started";

        recvPacket();
    }
    else {

        LOG_ERROR << error.message();


        close(false, true);


    }
}

// ============================================================ //

void ClientSession::onPacketSent(
        const boost::system::error_code &error,
        size_t,
        Zway::PACKET)
{
    if (!error) {

        resetTimer();

        m_numPacketsSent++;
    }
    else {

        LOG_ERROR << remoteHost() << " > onPacketSent: " << error.message();

        // ...
    }

    {
        boost::mutex::scoped_lock locker(m_sending);

        m_sending = false;
    }

    sendPacket();
}

// ============================================================ //

void ClientSession::onPacketRecv(Zway::PACKET pkt)
{
    m_numPacketsRecv++;

    /*
    if (pkt->id() == Zway::Packet::HeartbeatId) {

        resetTimer();

        postPacket(Zway::Packet::create(Zway::Packet::HeartbeatId));
    }
    else
    */
    if (pkt->streamId() && pkt->bodySize()) {

        if (processIncomingPacket(*pkt)) {

        }
        else {

            // ...
        }
    }
    else {


        LOG_ERROR << remoteHost() << " > invalid packet";

        // ...
    }

    recvPacket();
}

// ============================================================ //

void ClientSession::onPacketHeadRecv(
        const boost::system::error_code &error,
        size_t bytes_transferred,
        Zway::PACKET pkt)
{
    if (!error && bytes_transferred == sizeof(Zway::Packet::Head)) {

        resetTimer();

        if (pkt->bodySize() > 0) {

            recvPacketBody(pkt);
        }
        else {

            onPacketRecv(pkt);
        }
    }
    else {

        if (!error && bytes_transferred == 0) {

            // ...

            recvPacket();

            return;
        }
        else
        if (error == boost::asio::error::connection_aborted ||
            error == boost::asio::error::connection_reset ||
            error == boost::asio::error::eof ||
            error.value() == 0x140000DB) {

            close(false, true);

            return;
        }
        else
        if (error == boost::asio::error::operation_aborted) {

            return;
        }

        // ...
    }
}

// ============================================================ //

void ClientSession::onPacketBodyRecv(
        const boost::system::error_code &error,
        size_t bytes_transferred,
        size_t offset,
        Zway::PACKET pkt)
{
    if (!error && offset + bytes_transferred < pkt->bodySize()) {

        uint32_t packetOffset = offset + bytes_transferred;

        m_socket.async_read_some(
                boost::asio::buffer(pkt->body()->data() + packetOffset, pkt->bodySize() - packetOffset),
                boost::bind(
                    &ClientSession::onPacketBodyRecv,
                    shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred,
                    packetOffset,
                    pkt));
    }
    else
    if (!error && offset + bytes_transferred == pkt->bodySize()) {

        resetTimer();

        onPacketRecv(pkt);
    }
    else {

        if (error == boost::asio::error::connection_aborted ||
            error == boost::asio::error::connection_reset ||
            error == boost::asio::error::eof ||
            error.value() == 0x140000DB) {

            close(false, true);

            return;
        }
        else
        if (error == boost::asio::error::operation_aborted) {

            return;
        }

        // ...
    }
}

// ============================================================ //

bool ClientSession::sendPacket()
{
    {
        boost::mutex::scoped_lock locker(m_sending);

        if (m_sending) {

            return false;
        }
    }

    // grab first packet from queue

    Zway::PACKET pkt;

    {
        boost::mutex::scoped_lock locker(m_packetQueue);

        if (m_packetQueue->empty()) {

            Zway::Engine::processStreamSenders(true, [this] (Zway::PACKET pkt) -> bool {

                m_packetQueue->push(pkt);

                return true;
            });
        }

        if (!m_packetQueue->empty()) {

            pkt = m_packetQueue->front();

            m_packetQueue->pop();
        }
    }

    if (!pkt)  {

        return false;
    }

    std::vector<boost::asio::const_buffer> buffers;

    // add packet head

    buffers.push_back(boost::asio::buffer(&pkt->head(), sizeof(Zway::Packet::Head)));

    // add packet body

    if (pkt->bodySize() > 0) {

        buffers.push_back(boost::asio::buffer(pkt->bodyData(), pkt->bodySize()));
    }

    // send packet

    {
        boost::mutex::scoped_lock locker(m_sending);

        m_sending = true;
    }

    boost::asio::async_write(
            m_socket,
            buffers,
            boost::bind(
                &ClientSession::onPacketSent,
                shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred,
                pkt));

    return true;
}

// ============================================================ //

bool ClientSession::recvPacket()
{
    Zway::PACKET pkt = Zway::Packet::create();

    if (!pkt) {

        return false;
    }

    m_socket.async_read_some(
                boost::asio::buffer(&pkt->head(), sizeof(Zway::Packet::Head)),
                boost::bind(
                    &ClientSession::onPacketHeadRecv,
                    shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred,
                    pkt));

    return true;
}

// ============================================================ //

void ClientSession::recvPacketBody(Zway::PACKET pkt)
{
    Zway::BUFFER body = Zway::Buffer::create(nullptr, pkt->bodySize());

    if (!body) {

        // TODO invoke callback with error code

        return;
    }

    pkt->setBody(body);

    m_socket.async_read_some(
                boost::asio::buffer(body->data(), body->size()),
                boost::bind(
                    &ClientSession::onPacketBodyRecv,
                    shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred,
                    0,
                    pkt));
}

// ============================================================ //

bool ClientSession::processRequests()
{
    std::list<BSONObj> requests = DB::getRequests(BSON("dst" << accountId()));

    for (auto &request : requests) {

        uint32_t id = request["id"].numberInt();

        uint32_t type = request["type"].numberInt();

        if (requestPending((Zway::Request::Type)type, id)) {

            continue;
        }

        switch (type) {
        case Zway::Request::Dispatch: {

            Zway::UBJ::Object data = UBJ_OBJ(
                        "requestId"    << id <<
                        "requestType"  << type <<
                        "dispatchId"   << id <<
                        "dispatchType" << request["dispatchType"].numberInt());

            postRequest(DispatchRequest::create(
                            id, data,
                            [this,id] (DispatchRequest::Pointer request, const Zway::UBJ::Object &response) {

                uint32_t status = response["status"].toInt();

                if (status == 1) {

                    if (!DB::deleteRequest(BSON("id" << id << "dst" << accountId()))) {

                        // ...
                    }
                }

            }));

            break;
        }
        case Zway::Request::AddContact: {

            Zway::UBJ::Object data = UBJ_OBJ(
                        "requestId"   << id <<
                        "requestType" << type <<
                        "addCode"     << request["addCode"].str() <<
                        "name"        << request["name"].str() <<
                        "phone"       << request["phone"].str());

            postRequest(AddContactRequest::create(
                            id, data,
                            [this,id] (AddContactRequest::Pointer request, const Zway::UBJ::Object &response) {

            }));

            break;
        }
        case Zway::Request::AcceptContact: {

            Zway::UBJ::Object data = UBJ_OBJ(
                        "requestId"        << id <<
                        "requestType"      << type <<
                        "contactRequestId" << request["contactRequestId"].numberInt() <<
                        "contactId"        << request["src"].numberInt() <<
                        "contactStatus"    << getContactStatus(request["src"].numberInt()) <<
                        "name"             << request["name"].str() <<
                        "phone"            << request["phone"].str() <<
                        "publicKey"        << bsonToUbj(request["publicKey"]));

            // add contact

            {
                boost::mutex::scoped_lock lock(m_contacts);

                uint32_t requestSrc = request["src"].numberInt();

                (*m_contacts)[requestSrc] = UBJ_OBJ("contactId" << requestSrc << "notifyStatus" << 1);
            }

            postRequest(AcceptContactRequest::create(
                            id, data,
                            [this,id] (AcceptContactRequest::Pointer request, const Zway::UBJ::Object &response) {

                uint32_t status = response["status"].toInt();

                if (status == 1) {

                    if (!DB::deleteRequest(BSON("id" << id << "dst" << accountId()))) {

                        // ...
                    }
                }

            }));

            break;
        }
        case Zway::Request::RejectContact: {

            Zway::UBJ::Object data = UBJ_OBJ(
                        "requestId"        << id <<
                        "requestType"      << type <<
                        "contactRequestId" << request["contactRequestId"].numberInt());

            postRequest(RejectContactRequest::create(
                            id, data,
                            [this,id] (RejectContactRequest::Pointer request, const Zway::UBJ::Object &response) {

                uint32_t status = response["status"].toInt();

                if (status == 1) {

                    if (!DB::deleteRequest(BSON("id" << id << "dst" << accountId()))) {

                        // ...
                    }
                }

            }));

            break;
        }
        case Zway::Request::Push: {

            postRequest(PushRequest::create(
                            id, bsonToUbj(request["data"]),
                            [this,id] (PushRequest::Pointer, const Zway::UBJ::Object &response) {

                uint32_t status = response["status"].toInt();

                if (status == 1) {

                    if (!DB::deleteRequest(BSON("id" << id << "dst" << accountId()))) {

                        // ...
                    }

                    for (auto &it : response["resources"].toArray()) {

                        uint32_t id = it.toInt();

                        if (id) {

                            STREAM_BUFFER_SENDER sender = StreamBufferSender::create(m_server, shared_from_this(), id);

                            if (sender) {

                                if (addStreamSender(sender)) {

                                    m_server->addStreamBufferSender(sender);
                                }
                                else {

                                    // ...
                                }
                            }
                            else {

                                // ...
                            }
                        }
                    }
                }

            }));
        }
        }
    }

    return true;
}

// ============================================================ //

bool ClientSession::processDispatchRequest(const Zway::UBJ::Object &head)
{
    uint32_t requestId = head["requestId"].toInt();

    if (status() < STATUS_LOGGEDIN) {

        postRequestFailure(requestId, 0, "Operation not permitted");

        return false;
    }

    if (head.hasField("dispatchId")) {

        mongo::BSONObj query =
                BSON(
                    "id"  << head["dispatchId"].toInt() <<
                    "$or" << BSON_ARRAY(BSON("src" << accountId()) << BSON("dst" << accountId())));

        if (!DB::deleteRequest(query)) {

            postRequestFailure(requestId, 0, "invalid data");

            return false;
        }

        postRequestSuccess(requestId);

        return true;
    }

    return false;
}

// ============================================================ //

bool ClientSession::processCreateAccount(const Zway::UBJ::Object &head)
{
    uint32_t requestId = head["requestId"].toInt();

    if (status() >= STATUS_LOGGEDIN) {

        postRequestFailure(requestId, 0, "Operation not permitted");

        return false;
    }


    // TODO: input validation

    if (!head.hasField("name") || head["name"].toStr().empty()) {

        postRequestFailure(requestId, 0, "Missing field: name");

        return false;
    }

    if (!head.hasField("password") || head["password"].bufferSize() != 32) {

        postRequestFailure(requestId, 0, "Missing field: password");

        return false;
    }


    BSONObj account;

    if (DB::getAccount(BSON("name" << BSON("$regex" << head["name"].toStr() << "$options" << "i")), BSON("id" << 1), account)) {

        postRequestFailure(requestId, 0, "Invalid account name");

        return false;
    }


    uint32_t accountId = DB::newAccountId();


    SHA256_CTX ctx;

    if (!SHA256_Init(&ctx)) {

        // ...

        return false;
    }


    Zway::BUFFER password = head["password"].buffer()->copy();

    if (!SHA256_Update(&ctx, password->data(), password->size())) {

        return false;
    }


    Zway::BUFFER salt = Zway::Buffer::create(nullptr, 32);

    RAND_pseudo_bytes((uint8_t*)salt->data(), salt->size());

    if (!SHA256_Update(&ctx, salt->data(), salt->size())) {

        return false;
    }


    Zway::BUFFER pass = Zway::Buffer::create(nullptr, 32);

    if (!SHA256_Final(pass->data(), &ctx)) {

        return false;
    }


    // create account

    if (!DB::insertAccount(
                accountId,
                head["name"].toStr(),
                head["phone"].toStr(),
                head["findByName"].toBool(),
                head["findByPhone"].toBool(),
                BSONBinData(pass->data(), pass->size(), BinDataGeneral),
                BSONBinData(salt->data(), salt->size(), BinDataGeneral))) {

        postRequestFailure(requestId, 0, "Failed to create account");

        return false;
    }


    // send response

    postRequestSuccess(requestId, UBJ_OBJ("accountId" << accountId));

    return true;
}

// ============================================================ //

bool ClientSession::processLogin(const Zway::UBJ::Object &head)
{
    uint32_t requestId = head["requestId"].toInt();

    if (status() >= STATUS_LOGGEDIN) {

        postRequestFailure(requestId, 0, "Operation not permitted");

        return false;
    }

    // TODO input validation

    if (!head.hasField("account")) {

        postRequestFailure(requestId, 0, "account missing");

    	return false;
    }

    if (!head.hasField("password")) {

        postRequestFailure(requestId, 0, "password missing");

        return false;
    }


    uint32_t accountId = head["account"].toInt();


    BSONObj account;

    if (!DB::getAccount(BSON("id" << accountId), BSON("pass" << 1 << "salt" << 1), account)) {

        postRequestFailure(requestId, 0, "Invalid account id");

        return false;
    }


    if (!verifyPassword(account, head["password"].buffer())) {

        postRequestFailure(requestId, 0, "login failed");

        return false;
    }


    setStatus(STATUS_LOGGEDIN);

    // remove temporary session

    m_server->removeSession(shared_from_this());

    // append authenticated session

    m_accountId = accountId;

    m_server->appendSession(shared_from_this());


    // set config

    if (head.hasField("config")) {

        if (!setConfig(head["config"])) {

        }
    }


    Zway::UBJ::Array status;

    {
        boost::mutex::scoped_lock locker(m_contacts);

        for (auto &it : *m_contacts) {

            uint32_t contactId = it.first;

            uint32_t contactStatus = getContactStatus(contactId);

            status << UBJ_OBJ("contactId" << contactId << "status" << contactStatus);
        }
    }


    Zway::UBJ::Array inbox;

    DB::getInbox(inbox, accountId);


    // send response

    postRequestSuccess(requestId, UBJ_OBJ("contactStatus" << status << "inbox" << inbox));


    // process requests

    processRequests();


    LOG_INFO << remoteHost() << " > login > accountId: " << accountId;


    return true;
}

// ============================================================ //

bool ClientSession::processLogout(const Zway::UBJ::Object &head)
{
    uint32_t requestId = head["requestId"].toInt();

    if (status() < STATUS_LOGGEDIN) {

        postRequestFailure(requestId, 0, "Operation not permitted");

        return false;
    }

    postRequestSuccess(requestId);

    DB::setFcmToken(accountId(), std::string());


    setStatus(STATUS_CONNECTED);



    broadcastStatus(0);


    return true;
}

// ============================================================ //

bool ClientSession::processConfigRequest(const Zway::UBJ::Object &head)
{
    uint32_t requestId = head["requestId"].toInt();

    if (status() < STATUS_LOGGEDIN) {

        postRequestFailure(requestId, 0, "Operation not permitted");

        return false;
    }

    if (head.hasField("config")) {

        if (!setConfig(head["config"])) {

            postRequestFailure(requestId, 0, "failure");

            return false;
        }
    }

    postRequestSuccess(requestId);

    return true;
}

// ============================================================ //

bool ClientSession::processContactStatusRequest(const Zway::UBJ::Object &head)
{
    uint32_t requestId = head["requestId"].toInt();

    if (status() < STATUS_LOGGEDIN) {

        postRequestFailure(requestId, 0, "Operation not permitted");

        return false;
    }

    Zway::UBJ::Array status;

    {
        boost::mutex::scoped_lock locker(m_contacts);

        for (auto &it : *m_contacts) {

            uint32_t contactId = it.first;

            uint32_t contactStatus = getContactStatus(contactId);

            status << UBJ_OBJ("contactId" << contactId << "status" << contactStatus);
        }
    }

    postRequestSuccess(requestId, UBJ_OBJ("contactStatus" << status));

	return true;
}

// ============================================================ //

bool ClientSession::processFindContactRequest(const Zway::UBJ::Object &head)
{
    uint32_t requestId = head["requestId"].toInt();

    if (status() < STATUS_LOGGEDIN) {

        postRequestFailure(requestId, 0, "Operation not permitted");

        return false;
    }

    if (!head.hasField("query")) {

        postRequestFailure(requestId, 0, "query missing");

        return false;
    }

    Zway::UBJ::Object query = head["query"];

    BSONArray contacts;

    if (query.hasField("subject")) {

        std::string subject = query["subject"].toStr();

        BSONObj q = BSON(
                    "name" << BSON("$regex" << subject << "$options" << "i") <<
                    "id" << BSON("$ne" << accountId()) <<
                    "findByName" << true);

        BSONObj fieldsToReturn = BSON("name" << 1);

        contacts = DB::getContacts(q, &fieldsToReturn);
    }
    /*
    else
    if (query.hasField("numbers")) {

        BSONObj q = BSON(
                    "phone" << BSON("$in" << query["numbers"]) <<
                    "id" << BSON("$ne" << accountId()) <<
                    "findByPhone" << 1);

        BSONObj fieldsToReturn = BSON("name" << 1 << "phone" << 1);

        //contacts = db().getContacts(q, &fieldsToReturn);
    }
    */

    Zway::UBJ::Value result;

    postRequestSuccess(requestId, UBJ_OBJ("result" << bsonArrToUbj(contacts)));

    return true;
}

// ============================================================ //

bool ClientSession::processAddContactRequest(const Zway::UBJ::Object &head)
{
    uint32_t requestId = head["requestId"].toInt();

    if (status() < STATUS_LOGGEDIN) {

        postRequestFailure(requestId, 0, "Operation not permitted");

        return false;
    }

    // input validation

    // ...

    // get our account data

    BSONObj account;

    if (!DB::getAccount(BSON("id" << accountId()), BSON("name" << 1 << "phone" << 1), account)) {

        postRequestFailure(requestId, 0, "Internal server error");

        return false;
    }

    BSONObj contactQuery;

    // process add code if supplied

    std::string addCode = head["addCode"].toStr();

    if (!addCode.empty()) {
        
        // get request by add code in order to get contact id

        BSONObj request;

        if (!DB::getRequest(BSON("type" << Zway::Request::AddContact << "addCode" << addCode), request)) {

            postRequestFailure(requestId, 0, "invalid add code");

        	return false;
        }

        // query contact by account id

        contactQuery = BSON("id" << request["src"]);
    }
    else {

        std::string label = head["name"].toStr();

        if (label.empty()) {

            postRequestFailure(requestId, 0, "no label");

            return false;
        }

        // query contact by label

        contactQuery = BSON("name" << label << "findByName" << true);
    }

    BSONObj contact;

    if (!DB::getAccount(contactQuery, BSON("id" << 1 << "name" << 1 << "phone" << 1), contact)) {

        postRequestFailure(requestId, 0, "invalid label 1");

        return false;
    }

	uint32_t contactAccountId = contact["id"].numberInt();

    if (contactAccountId == accountId()) {

        postRequestFailure(requestId, 0, "invalid label 2");

        return false;
    }

    if (DB::requestPending(
            BSON(
                "type" << Zway::Request::AddContact <<
                "src"  << accountId() <<
                "dst"  << contactAccountId))) {

        postRequestFailure(requestId, 0, "invalid data");

		return false;
	}

    mongo::BSONObj publicKey;

    bool hasPublicKey = head.hasField("publicKey");

    if (hasPublicKey) {

        try {

            publicKey = ubjToBson(head["publicKey"]);
        }
        catch (std::exception &e) {

            hasPublicKey = false;
        }
    }

    if (!hasPublicKey) {

        postRequestFailure(requestId, 0, "Invalid public key");

        return false;
    }

    if (!DB::addRequest(
            BSON(
                "id"        << requestId <<
                "type"      << Zway::Request::AddContact <<
                "time"      << 0 <<
                "ttl"       << 0 <<
                "src"       << accountId() <<
                "dst"       << contactAccountId <<
                "name"      << account["name"] <<
                "phone"     << account["phone"] <<
                "publicKey" << publicKey <<
                "addCode"   << addCode))) {

        postRequestFailure(requestId, 0, "Internal server error");

		return false;
	}

    // delete add code if any 

    if (!addCode.empty()) {

        DB::deleteRequest(BSON("addCode" << addCode));
    }

    // send response

    postRequestSuccess(
                requestId,
                UBJ_OBJ(
                    "requestId"    << requestId <<
                    "status"       << 1 <<
                    "addCode"      << addCode <<
                    "name"         << contact["name"].str() <<
                    "phone"        << contact["phone"].str()));

    // process requests for the requested user

    m_server->io_service()->post(boost::bind(&Server::processUserRequests, m_server, contactAccountId));

    return true;
}

// ============================================================ //

bool ClientSession::processCreateAddCode(const Zway::UBJ::Object &head)
{
    uint32_t requestId = head["requestId"].toInt();

    if (status() < STATUS_LOGGEDIN) {

        postRequestFailure(requestId, 0, "Operation not permitted");

        return false;
    }

    mongo::BSONObj publicKey;

    bool hasPublicKey = head.hasField("publicKey");

    if (hasPublicKey) {

        try {

            publicKey = ubjToBson(head["publicKey"]);
        }
        catch (std::exception &e) {

            hasPublicKey = false;
        }
    }

    if (!hasPublicKey) {

        postRequestFailure(requestId, 0, "Invalid public key");

        return false;
    }

    // get our account data

    BSONObj account;

    if (!DB::getAccount(BSON("id" << accountId()), BSON("name" << 1 << "phone" << 1), account)) {

        postRequestFailure(requestId, 0, "Internal server error");

        return false;
    }

    // create add code

    std::string addCode;

    addCode.resize(sizeof(uint32_t));

    RAND_pseudo_bytes((uint8_t*)&addCode[0], sizeof(uint32_t));

    std::string addCodeHex = boost::algorithm::hex(addCode);

    if (!DB::addRequest(
            BSON(
                "id"        << requestId <<
                "type"      << Zway::Request::AddContact <<
                "time"      << 0 <<
                "ttl"       << 0 <<
                "src"       << accountId() <<
                "name"      << account["name"] <<
                "phone"     << account["phone"] <<
                "publicKey" << publicKey <<
                "addCode"   << addCodeHex))) {

        postRequestFailure(requestId, 0, "Internal server error");

        return false;
    }

    // send response

    postRequestSuccess(
                requestId,
                UBJ_OBJ(
                    "requestId"    << requestId <<
                    "status"       << 1 <<
                    "addCode"      << addCodeHex));

    return true;
}

// ============================================================ //

bool ClientSession::processAcceptContact(const Zway::UBJ::Object &head)
{
    uint32_t requestId = head["requestId"].toInt();

    if (status() < STATUS_LOGGEDIN) {

        postRequestFailure(requestId, 0, "Operation not permitted");

        return false;
    }

    // get contact request

    uint32_t contactRequestId = head["contactRequestId"].toInt();

    BSONObj request;

    if (!DB::getRequest(BSON("id" << contactRequestId << "dst" << accountId()), request)) {

        postRequestFailure(requestId, 1, "Invalid request");

    	return false;
    }

    // get account data

    BSONObj account;

    if (!DB::getAccount(BSON("id" << accountId()), BSON("name" << 1 << "phone" << 1), account)) {

        postRequestFailure(requestId, 0, "Internal server error");

        return false;
    }

    uint32_t requestSrc = request["src"].numberInt();

    Zway::UBJ::Object contactPublicKey = bsonObjToUbj(request["publicKey"].Obj());

    BSONObj publicKey;

    bool hasPublicKey = head.hasField("publicKey");

    if (hasPublicKey) {

        try {

            publicKey = ubjToBson(head["publicKey"]);
        }
        catch (std::exception &e) {

            hasPublicKey = false;
        }
    }

    if (!hasPublicKey) {

        postRequestFailure(requestId, 0, "Invalid public key");

        return false;
    }

    /*
    if (DB::requestPending(
            BSON(
                "type" << Zway::Request::AcceptContactType <<
                "src"  << accountId() <<
                "dst"  << requestSrc))) {

        postRequestFailure(requestId, 0, "Invalid request");

        return false;
    }
    */

    // add request for accepted contact

    if (!DB::addRequest(
			BSON(
                "id"               << requestId <<
                "contactRequestId" << contactRequestId <<
                "type"             << Zway::Request::AcceptContact <<
                "src"              << accountId() <<
                "dst"              << requestSrc <<
                "name"             << account["name"] <<
                "phone"            << account["phone"] <<
                "publicKey"        << publicKey))) {

        postRequestFailure(requestId, 0, "Internal server error");

		return false;
	}


    // send response

    postRequestSuccess(
                requestId,
                UBJ_OBJ(
                    "requestId"     << requestId <<
                    "status"        << 1 <<
                    "contactId"     << requestSrc <<
                    "contactStatus" << getContactStatus(requestSrc) <<
                    "name"          << request["name"].str() <<
                    "phone"         << request["phone"].str() <<
                    "publicKey"     << contactPublicKey));


    // delete contact request

    DB::deleteRequest(BSON("id" << contactRequestId));

    // add contact

    {
        boost::mutex::scoped_lock lock(m_contacts);

        (*m_contacts)[requestSrc] = UBJ_OBJ("contactId" << requestSrc << "notifyStatus" << 1);
    }

    // process requests for the accepted contact

    m_server->io_service()->post(boost::bind(&Server::processUserRequests, m_server, requestSrc));

	return true;
}

// ============================================================ //

bool ClientSession::processRejectContact(const Zway::UBJ::Object &head)
{
    uint32_t requestId = head["requestId"].toInt();

    if (status() < STATUS_LOGGEDIN) {

        postRequestFailure(requestId, 0, "Operation not permitted");

        return false;
    }

    // TODO input validation

    // get contact request

    uint32_t contactRequestId = head["contactRequestId"].toInt();

    BSONObj request;

    if (!DB::getRequest(BSON("id" << contactRequestId << "dst" << accountId()), request)) {

        postRequestFailure(requestId, 1, "Invalid request");

    	return false;
    }

    uint32_t requestSrc = request["src"].numberInt();

    /*
    if (DB::requestPending(
            BSON(
                "type" << Zway::Request::RejectContactType <<
                "src"  << accountId() <<
                "dst"  << requestSrc))) {

        postRequestFailure(requestId, 2, "Invalid request");

        return false;
    }
    */

    // add request for rejected contact

    if (!DB::addRequest(
            BSON(
                "type"             << Zway::Request::RejectContact <<
                "id"               << requestId <<
                "contactRequestId" << contactRequestId <<
                "src"              << accountId() <<
                "dst"              << requestSrc))) {

        postRequestFailure(requestId, 0, "Internal server error");

        return false;
	}


    // send response

    postRequestSuccess(requestId);


    // delete contact request

    DB::deleteRequest(BSON("id" << contactRequestId));


    // process requests for the rejected contact

    m_server->io_service()->post(boost::bind(&Server::processUserRequests, m_server, requestSrc));

	return true;
}

// ============================================================ //

bool ClientSession::processPushRequest(const Zway::UBJ::Object &head)
{
    uint32_t requestId = head["requestId"].toInt();

    if (status() < STATUS_LOGGEDIN) {

        postRequestFailure(requestId, 0, "Operation not permitted");

        return false;
    }


    // check request

    // ...

    Zway::UBJ::Array resources = head["resources"];

    // TODO resources allowed to send

    Zway::UBJ::Array resourceIds;

    for (auto &it : resources) {

        resourceIds << it["id"].toInt();
    }

    Zway::UBJ::Object response = UBJ_OBJ("resources" << resourceIds);


    // store request

    //storeInsert(messageId, request, true);


    // forward request

    // TODO create new request id

    auto forward = UBJ_OBJ(
                "requestId"   << requestId <<
                "requestType" << head["requestType"] <<
                "src"         << accountId() <<
                "resources"   << head["resources"] <<
                "salt"        << head["salt"] <<
                "meta"        << head["meta"]);

    Zway::UBJ::Array keys = head["keys"];

    for (auto &it : keys) {

        uint32_t dst = it["dst"].toInt();

        if (dst == accountId()) {

            // ...

            continue;
        }

        forward["key"] = it["key"];

        if (!DB::addRequest(
                BSON(
                    "id"        << requestId <<
                    "type"      << Zway::Request::Push <<
                    "time"      << 0 <<
                    "ttl"       << 0 <<
                    "src"       << accountId() <<
                    "dst"       << dst <<
                    "data"      << ubjToBson(forward)))) {

            postRequestFailure(requestId, 0, "Internal server error");

            // ...

            continue;
        }

        m_server->io_service()->post(boost::bind(&Server::processUserRequests, m_server, dst));
    }

    // send response

    postRequestSuccess(requestId, response);

    return true;
}

// ============================================================ //

void ClientSession::broadcastStatus(uint32_t status, bool check)
{
    if (check && !m_config["notifyStatus"].toBool()) {

        return;
    }

    // notify online contacts about status change

    boost::mutex::scoped_lock locker(m_contacts);

    /*
    BSONObjBuilder builder;

    builder.append(boost::lexical_cast<std::string>(m_accountId), BSON("status" << status));

    BSONObj contactStatus = builder.obj();
    */

    for (auto &it : *m_contacts) {

        uint32_t contactId = it.first;

        if (it.second["notifyStatus"].toBool()) {

            CLIENT_SESSION_LIST sessions = m_server->getSessions(contactId);

            if (!sessions.empty()) {

                Zway::UBJ::Array contactStatus = {
                    UBJ_OBJ("contactId" << accountId() << "status" << status)
                };

                Zway::UBJ::Object obj = UBJ_OBJ(
                            "requestType"   << Zway::Request::ContactStatus <<
                            "contactStatus" << contactStatus);

                uint32_t requestId;

                RAND_pseudo_bytes((uint8_t*)&requestId, sizeof(requestId));

                for (auto &session : sessions) {

                    session->addUbjSender(requestId, Zway::Packet::Request, obj);
                }
            }
        }
    }
}

// ============================================================ //

uint32_t ClientSession::getContactStatus(uint32_t contactId)
{
    CLIENT_SESSION_LIST sessions = m_server->getSessions(contactId);

    for (CLIENT_SESSION &s: sessions) {

        if (s->config()["notifyStatus"].toBool()) {

            return 1;
        }
    }

    return 0;
}

// ============================================================ //

bool ClientSession::postPacket(Zway::PACKET pkt)
{
    if (!pkt) {

        return false;
    }

    {
        boost::mutex::scoped_lock locker(m_packetQueue);

        m_packetQueue->push(pkt);
    }

    sendPacket();

    return true;
}

// ============================================================ //

bool ClientSession::addStreamSender(Zway::STREAM_SENDER sender)
{
    if (!Zway::Engine::addStreamSender(sender)) {

        return false;
    }

    sendPacket();

    return true;
}

// ============================================================ //

bool ClientSession::processIncomingRequest(const Zway::UBJ::Object &request)
{
    switch (request["requestType"].toInt()) {

        case Zway::Request::Dispatch:

            return processDispatchRequest(request);

        case Zway::Request::CreateAccount:

            return processCreateAccount(request);

        case Zway::Request::Login:

            return processLogin(request);

        case Zway::Request::Logout:

            return processLogout(request);

        case Zway::Request::Config:

            return processConfigRequest(request);

        case Zway::Request::AddContact:

            return processAddContactRequest(request);

        case Zway::Request::CreateAddCode:

            return processCreateAddCode(request);

        case Zway::Request::FindContact:

            return processFindContactRequest(request);

        case Zway::Request::AcceptContact:

            return processAcceptContact(request);

        case Zway::Request::RejectContact:

            return processRejectContact(request);

        case Zway::Request::ContactStatus:

            return processContactStatusRequest(request);

        case Zway::Request::Push:

            return processPushRequest(request);

        /*
        case Request::Pull:

            return processGetResource(request);
            */
    }

    return false;
}

// ============================================================ //

Zway::STREAM_RECEIVER ClientSession::createStreamReceiver(const Zway::Packet &pkt)
{
    if (pkt.streamType() == Zway::Packet::Resource) {

        // TODO check stream info


        // get resource record


        // ...


        // create stream buffer

        std::stringstream ss;

        ss << "tmp/" << pkt.streamId();

        STREAM_BUFFER buffer = StreamBuffer::create(ss.str(), pkt);

        if (!buffer) {

            return nullptr;
        }

        // create stream receiver

        auto receiver = Zway::BufferReceiver::create(
                    pkt, buffer,
                    [this] (Zway::BUFFER_RECEIVER receiver, Zway::BUFFER buffer, size_t /*bytesReceived*/) {


                        // TODO move to buffer

                        buffer->flush();


                        // dispatch request

                        if (!DB::addRequest(
                                BSON(
                                    "id"          << receiver->id() <<
                                    "type"         << Zway::Request::Dispatch <<
                                    "time"         << 0 <<
                                    "ttl"          << 0 <<
                                    "src"          << 0 <<
                                    "dst"          << accountId() <<
                                    "dispatchType" << 6))) {

                            // ...
                        }

                        processRequests();

                    });

        if (receiver) {

            if (!m_server->addStreamBuffer(buffer)) {

                // ...
            }

            return receiver;
        }
    }
    else {

        return Zway::Engine::createStreamReceiver(pkt);
    }

    return nullptr;
}

// ============================================================ //

Zway::UBJ::Object &ClientSession::config()
{
    return m_config;
}

// ============================================================ //

bool ClientSession::verifyPassword(const mongo::BSONObj &account, Zway::BUFFER password)
{
    int saltLen = 0;
    int passLen = 0;

    // TODO check the following for memory leaks

    const char *saltData = account["salt"].binData(saltLen);
    const char *passData = account["pass"].binData(passLen);

    SHA256_CTX ctx;

    if (!SHA256_Init(&ctx)) {

        return false;
    }

    if (!SHA256_Update(&ctx, password->data(), password->size())) {

        return false;
    }

    if (!SHA256_Update(&ctx, saltData, saltLen)) {

        return false;
    }

    Zway::BUFFER digest = Zway::Buffer::create(nullptr, 32);

    if (!digest) {

        return false;
    }

    if (!SHA256_Final(digest->data(), &ctx)) {

        return false;
    }

    if (memcmp(digest->data(), passData, passLen)) {

        return false;
    }

    return true;
}

// ============================================================ //

uint32_t ClientSession::status()
{
    boost::mutex::scoped_lock locker(m_status);

    return m_status;
}

// ============================================================ //

std::string ClientSession::remoteHost()
{
    return m_remoteHost;
}

// ============================================================ //

uint32_t ClientSession::accountId()
{
    return m_accountId;
}

// ============================================================ //

ssl_socket::lowest_layer_type& ClientSession::socket()
{
    return m_socket.lowest_layer();
}

// ============================================================ //

void ClientSession::ubjValToBson(const std::string &key, const Zway::UBJ::Value &val, mongo::BSONObjBuilder &ob)
{
    switch (val.type()) {
    case UBJ_OBJECT:
        ob.append(key, ubjToBson(val));
        break;
    case UBJ_ARRAY:
        if (val.bufferSize()) {
            mongo::BSONBinData binData = mongo::BSONBinData(val.bufferData(), val.bufferSize(), mongo::BinDataGeneral);
            ob.append(key, binData);
        }
        else {
            mongo::BSONArray arr = mongo::BSONArray(ubjToBson(val));
            ob.append(key, arr);
        }
        break;
    case UBJ_STRING:
        ob.append(key, val.toStr());
        break;
    case UBJ_INT32:
        ob.append(key, val.toInt());
        break;
    case UBJ_INT64:
        ob.append(key, (long long)val.toLong());
        break;
    case UBJ_BOOL_TRUE:
    case UBJ_BOOL_FALSE:
        ob.append(key, val.toBool());
        break;
    }
}

// ============================================================ //

mongo::BSONObj ClientSession::ubjToBson(const Zway::UBJ::Value &ubj)
{
    mongo::BSONObj res;

    mongo::BSONObjBuilder ob;

    if (ubj.isObject()) {

        for (auto &it : ubj.obj()) {

            try {

                ubjValToBson(it.first, it.second, ob);
            }
            catch (std::runtime_error &e) {

                // ...
            }
        }

        res = ob.obj();
    }
    else
    if (ubj.isArray()) {

        for (auto &it : ubj.arr()) {

            try {

                ubjValToBson(std::string(), it, ob);
            }
            catch (std::runtime_error &e) {

                // ...
            }
        }

        res = mongo::BSONArray(ob.obj());
    }

    return res;
}

// ============================================================ //

Zway::UBJ::Value ClientSession::bsonToUbj(const mongo::BSONElement &ele)
{
    Zway::UBJ::Value val;

    switch (ele.type()) {
    case mongo::Object:
        val = bsonObjToUbj(ele.Obj());
        break;
    case mongo::Array:
        val = bsonArrToUbj(ele.Obj());
        break;
    case mongo::String:
        val = ele.str();
        break;
    case mongo::NumberInt:
        val = (int32_t)ele.numberInt();
        break;
    case mongo::NumberLong:
        val = (int64_t)ele.numberLong();
        break;
    case mongo::NumberDouble:
        val = (double)ele.numberDouble();
        break;
    case mongo::Bool:
        val = ele.boolean();
        break;
    case mongo::BinData: {

        int32_t len = 0;

        uint8_t* binData = (uint8_t*)ele.binData(len);

        if (binData && len) {

            Zway::BUFFER buf = Zway::Buffer::create(binData, len);

            if (buf) {

                val = buf;
            }
        }
        break;
    }
    default:
        break;
    }

    return val;
}

// ============================================================ //

Zway::UBJ::Object ClientSession::bsonObjToUbj(const mongo::BSONObj &obj)
{
    Zway::UBJ::Object res;

    mongo::BSONObjIterator it = obj.begin();

    while (it.more()) {

        mongo::BSONElement ele = it.next();

        res[ele.fieldName()] = bsonToUbj(ele);
    }

    return res;
}

// ============================================================ //

Zway::UBJ::Array ClientSession::bsonArrToUbj(const mongo::BSONObj &arr)
{
    Zway::UBJ::Array res;

    mongo::BSONObjIterator it = arr.begin();

    while (it.more()) {

        mongo::BSONElement ele = it.next();

        res << bsonToUbj(ele);
    }

    return res;
}

// ============================================================ //
