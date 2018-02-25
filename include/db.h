
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

#ifndef DB_H_
#define DB_H_

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

#include <mongo/client/dbclient.h>
#include <mongo/client/gridfs.h>

#include <queue>

#include <Zway/core/ubj/value.h>

// ============================================================ //

class DB
{
public:

    class Connection
    {
    public:

        typedef boost::shared_ptr<Connection> Pointer;

        class Lock
        {

        public:

            typedef boost::shared_ptr<Lock> Pointer;

            static Pointer create(Connection::Pointer con);

            ~Lock();

            void unlock();

            Pointer connection();

            mongo::DBClientConnection *db();

        protected:

            Lock(Connection::Pointer con);

        protected:

            Connection::Pointer m_con;
        };

        typedef Lock::Pointer LOCK;

        static Pointer create(const std::string &address);

        mongo::DBClientConnection *db();

    protected:

        Connection();

        bool init(const std::string &address);

        boost::shared_ptr<mongo::DBClientConnection> m_db;

        boost::shared_ptr<mongo::GridFS> m_fs;
    };

    typedef Connection::Pointer CONNECTION;

    static bool startup(const std::string& address, uint32_t numConnections = 10);

    static void cleanup();

    static Connection::LOCK acquire();


    static uint32_t newAccountId();

    static bool getAccount(const mongo::BSONObj& query, const mongo::BSONObj &fieldsToReturn, mongo::BSONObj& res);

    static bool insertAccount(
            uint32_t id,
            const std::string &name,
            const std::string &phone,
            bool findByName,
            bool findByPhone,
            const mongo::BSONBinData &pass,
            const mongo::BSONBinData &salt);

    static bool deleteAccount(const mongo::BSONObj& info);


    static bool addRequest(const mongo::BSONObj& data);

    static bool deleteRequest(const mongo::BSONObj& query);

    static bool requestPending(const mongo::BSONObj& query);


    static bool getRequest(const mongo::BSONObj& query, mongo::BSONObj& res);

    static std::list<mongo::BSONObj> getRequests(const mongo::BSONObj& query);


    static mongo::BSONArray getContacts(const mongo::BSONObj& query, mongo::BSONObj* fieldsToReturn = NULL);


    static bool getInbox(Zway::UBJ::Array &inbox, uint32_t accountId, uint32_t filter=0);


    static bool comparePhone(const std::string& p1, const std::string& p2);


    static bool setFcmToken(uint32_t accountId, const std::string &token);

    static std::string getFcmToken(uint32_t accountId);


    static uint32_t numContactRequests(uint32_t accountId);

    static uint32_t numPushRequests(uint32_t accountId);


protected:

    static CONNECTION getConnection();

protected:

    static boost::mutex m_mutex;

    static boost::condition_variable m_condition;

    static boost::mutex m_connectionsMutex;

    static std::queue<Connection::Pointer> m_connections;
};

// ============================================================ //

#endif /* DB_H_ */
