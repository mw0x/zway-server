
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

#include "db.h"
#include "logger.h"

#include <boost/lexical_cast.hpp>

using namespace mongo;

boost::mutex DB::m_mutex;

boost::condition_variable DB::m_condition;

boost::mutex DB::m_connectionsMutex;

std::queue<DB::Connection::Pointer> DB::m_connections;

// ============================================================ //
// DB
// ============================================================ //

bool DB::startup(const std::string& address, uint32_t numConnections)
{
    mongo::Status status = mongo::client::initialize();

    if (!status.isOK()) {

        return false;
    }

    for (int32_t i=0; i<numConnections; ++i) {

        Connection::Pointer pc = Connection::create(address);

        if (!pc) {

            return false;
        }

        m_connections.push(pc);
    }

    return true;
}

// ============================================================ //

void DB::cleanup()
{
    // ...

    m_connections = std::queue<CONNECTION>();

    mongo::client::shutdown();
}

// ============================================================ //

DB::Connection::LOCK DB::acquire()
{
    CONNECTION con = getConnection();

    if (!con) {

        boost::mutex::scoped_lock locker(m_mutex);

        m_condition.wait_for(locker, boost::chrono::seconds(10));

        con = getConnection();
    }

    if (con) {

        return Connection::Lock::create(con);
    }

    return nullptr;
}

// ============================================================ //

DB::CONNECTION DB::getConnection()
{
    CONNECTION con;

    boost::mutex::scoped_lock locker(m_connectionsMutex);

    if (!m_connections.empty()) {

        con = m_connections.front();

        m_connections.pop();
    }

    return con;
}

// ============================================================ //

DB::Connection::Lock::Pointer DB::Connection::Lock::create(DB::Connection::Pointer con)
{
    return LOCK(new Lock(con));
}

DB::Connection::Lock::Lock(DB::Connection::Pointer con)
    : m_con(con)
{

}

DB::Connection::Lock::~Lock()
{
    unlock();
}

void DB::Connection::Lock::unlock()
{
    {
        boost::mutex::scoped_lock locker(m_connectionsMutex);

        m_connections.push(m_con);
    }

    boost::mutex::scoped_lock locker(m_mutex);

    m_condition.notify_one();
}

DBClientConnection *DB::Connection::Lock::db()
{
    return m_con->db();
}

// ============================================================ //

DB::Connection::Pointer DB::Connection::create(const std::string &address)
{
    Pointer p(new Connection());

    if (!p->init(address)) {

        return nullptr;
    }

    return p;
}

// ============================================================ //

DBClientConnection *DB::Connection::db()
{
    return m_db.get();
}

// ============================================================ //

DB::Connection::Connection()
{

}

// ============================================================ //

bool DB::Connection::init(const std::string &address)
{
    try {

        m_db = boost::make_shared<mongo::DBClientConnection>();

        m_db->connect(address);

        std::string err;

        m_db->auth("zway", "admin", "123456", err);

        m_fs = boost::make_shared<mongo::GridFS>(*m_db, "zway");
    }
    catch (std::exception& e) {

        LOG_ERROR << e.what();

        return false;
    }

    return true;
}

// ============================================================ //

uint32_t DB::newAccountId()
{
    Connection::LOCK lock = acquire();

    try {

        BSONObj fieldsToReturn = BSON("id" << 1);

        std::unique_ptr<DBClientCursor> cursor = lock->db()->query("zway.accounts", Query().sort("id", -1), 1, 0, &fieldsToReturn);

        if (cursor->more()) {

            return cursor->next().getIntField("id") + 1;
        }
    }
    catch (std::exception& e) {

        LOG_ERROR << "Query failed: " << e.what();

        return 0;
    }

    return 1;
}

// ============================================================ //

bool DB::getAccount(const BSONObj &query, const mongo::BSONObj &fieldsToReturn, BSONObj &res)
{
    Connection::LOCK lock = acquire();

    try {

        std::unique_ptr<DBClientCursor> cursor = lock->db()->query("zway.accounts", query, 0, 0, &fieldsToReturn);

        if (cursor->more()) {
            
            res = cursor->next().copy();

            return true;
        }
    }
    catch (std::exception& e) {

        LOG_ERROR << "Failed to get account: " << e.what();
    }

    return false;
}

// ============================================================ //

bool DB::comparePhone(const std::string& p1, const std::string& p2)
{
	if (p1.empty() || p2.empty()) {

		return false;
	}

	return (p1.find(p2) != std::string::npos ||
            p2.find(p1) != std::string::npos);
}

// ============================================================ //

bool DB::setFcmToken(uint32_t accountId, const std::string &token)
{
    Connection::LOCK lock = acquire();

    try {

        lock->db()->update("zway.accounts", BSON("id" << accountId), BSON("$set" << BSON("fcmToken" << token)));

        return true;
    }
    catch (std::exception& e) {

        LOG_ERROR << "Failed to set fcm token: " << e.what();
    }

    return false;
}

// ============================================================ //

std::string DB::getFcmToken(uint32_t accountId)
{
    Connection::LOCK lock = acquire();

    try {

        BSONObj fieldsToReturn = BSON("fcmToken" << 1);

        std::unique_ptr<DBClientCursor> cursor = lock->db()->query("zway.accounts", BSON("id" << accountId), 0, 0, &fieldsToReturn);

        if (cursor->more()) {

            BSONObj obj = cursor->next();

            std::string token = obj["fcmToken"].String();

            return token;
        }
    }
    catch (std::exception& e) {

        LOG_ERROR << "Failed to get fcm token: " << e.what();
    }

    return std::string();
}

// ============================================================ //

uint32_t DB::numContactRequests(uint32_t accountId)
{
    Connection::LOCK lock = acquire();

    uint32_t res = -1;

    try {

        res = lock->db()->count("zway.requests", BSON("dst" << accountId << "type" << 3100));
    }
    catch (std::exception& e) {

        LOG_ERROR << e.what();
    }

    return res;
}

// ============================================================ //

uint32_t DB::numPushRequests(uint32_t accountId)
{
    Connection::LOCK lock = acquire();

    uint32_t res = -1;

    try {

        res = lock->db()->count("zway.requests", BSON("dst" << accountId << "type" << 4100));
    }
    catch (std::exception& e) {

        LOG_ERROR << e.what();
    }

    return res;
}

// ============================================================ //

bool DB::insertAccount(
        uint32_t id,
        const std::string &name,
        const std::string &phone,
        bool findByName,
        bool findByPhone,
        const BSONBinData &pass,
        const BSONBinData &salt)
{
    Connection::LOCK lock = acquire();

    try {

        lock->db()->insert("zway.accounts",
    		BSON(
                "id"          << id <<
                "name"        << name <<
                "phone"       << phone <<
                "findByName"  << findByName <<
                "findByPhone" << findByPhone <<
                "pass"        << pass <<
                "salt"        << salt));

        return true;
    }
    catch (std::exception& e) {

        LOG_ERROR << "Failed to insert account: " << e.what();
    }

    return false;
}

// ============================================================ //

bool DB::deleteAccount(const BSONObj& info)
{
	/*
	boost::mutex::scoped_lock l(m_db);

    if (!verifyAccount(info)) {

        return false;
    }

    try {

        m_db->remove("zway.accounts", QUERY(
        		"username" << info["username"].str() <<
        		"password" << info["password"].str()),
        		true);

        return true;
    }
    catch (std::exception& e) {

        rError("Failed to delete account: %s", e.what());
    }
    */

    return false;
}

// ============================================================ //

bool DB::addRequest(const BSONObj& data)
{
    Connection::LOCK lock = acquire();

    try {

        lock->db()->insert("zway.requests", data);

        return true;
    }
    catch (std::exception& e) {

        LOG_ERROR << "Failed to add request: " << e.what();
    }

	return true;
}

// ============================================================ //

bool DB::deleteRequest(const BSONObj& query)
{
    Connection::LOCK lock = acquire();

    try {

        lock->db()->remove("zway.requests", query, true);

        return true;
    }
    catch (std::exception& e) {

        LOG_ERROR << "Failed to delete request: " << e.what();
    }

    return false;
}

// ============================================================ //

bool DB::requestPending(const BSONObj& query)
{
    Connection::LOCK lock = acquire();

	try {

        if (lock->db()->count("zway.requests", query) == 1) {

			return true;
		}
	}
	catch (std::exception& e) {

        LOG_ERROR << e.what();
	}

	return false;
}

// ============================================================ //

bool DB::getRequest(const BSONObj& query, BSONObj& res)
{
    Connection::LOCK lock = acquire();

    try {

        //BSONObj fieldsToReturn = BSON("contacts" << 1);

        std::unique_ptr<DBClientCursor> cursor = lock->db()->query("zway.requests", query, 0, 0/*, &fieldsToReturn*/);

        if (cursor->more()) {

            BSONObj obj = cursor->next();

            res = obj.copy();

            return true;
        }
    }
    catch (std::exception& e) {

        LOG_ERROR << "Failed to get request: " << e.what();
    }

    return false;
}

// ============================================================ //

std::list<BSONObj> DB::getRequests(const BSONObj& query)
{
    Connection::LOCK lock = acquire();

    std::list<BSONObj> res;

    try {

        //BSONObj fieldsToReturn = BSON("contacts" << 1);

        std::unique_ptr<DBClientCursor> cursor = lock->db()->query("zway.requests", query, 0, 0/*, &fieldsToReturn*/);

        while (cursor->more()) {

            res.push_back(cursor->next().copy());
        }
    }
    catch (std::exception& e) {

        LOG_ERROR << "Failed to get requests: " << e.what();
    }

    return res;
}

// ============================================================ //

BSONArray DB::getContacts(const BSONObj& query, BSONObj* fieldsToReturn)
{
    Connection::LOCK lock = acquire();

    BSONArrayBuilder builder;

    try {

        std::unique_ptr<DBClientCursor> cursor = lock->db()->query("zway.accounts", query, 50, 0, fieldsToReturn);

        while (cursor->more()) {

            builder.append(cursor->next());
        }
    }
    catch (std::exception& e) {

        LOG_ERROR << e.what();
    }

    return builder.arr();
}

// ============================================================ //

bool DB::getInbox(Zway::UBJ::Array &inbox, uint32_t accountId, uint32_t filter)
{
    Connection::LOCK lock = acquire();

    std::map<uint32_t, Zway::UBJ::Array> map;

	try {

        BSONObj query = BSON("dst" << accountId << "type" << 4100);

        BSONObj fieldsToReturn = BSON("src" << 1 << "id" << 1);

        std::unique_ptr<DBClientCursor> cursor = lock->db()->query("zway.requests", query, 0, 0, &fieldsToReturn);

        while (cursor->more()) {

            BSONObj obj = cursor->next();

            uint32_t src =  obj["src"].numberInt();

            if (map.find(src) == map.end()) {

                map[src] = {obj["id"].numberInt()};
            }
            else {

                map[src] << obj["id"].numberInt();
            }
        }

        Zway::UBJ::Array res;

        for (auto &it : map) {

            res << UBJ_OBJ("contactId" << it.first << "requestIds" << it.second);
        }

        inbox = res;
	}
	catch (std::exception& e) {

        return false;
	}

    return true;
}

// ============================================================ //
