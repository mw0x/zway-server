
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

#ifndef SERVER_FCM_SENDER_H_
#define SERVER_FCM_SENDER_H_

#include <curl/curl.h>

#include <string>

// ============================================================ //

class FcmSender
{

public:

    static const char* fcmUrl;
    static const char* fcmServerKey;

    struct WriteThis {
        const char *readptr;
        long sizeLeft;
    };


    static bool startup();

    static void cleanup();


    static bool sendMessage(const std::string &token, uint32_t type, uint32_t numElements);


    static size_t readCallback(void *ptr, size_t size, size_t nmemb, void *userp);

    static size_t writeCallback(void *ptr, size_t size, size_t nmemb, void *userp);

};

// ============================================================ //

#endif

