
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

#include "fcmsender.h"
#include "logger.h"

#include <sstream>

const char *FcmSender::fcmUrl = "https://fcm.googleapis.com/fcm/send";
const char *FcmSender::fcmServerKey = "AAAAPYes6ds:APA91bEy1dPjayq9vOzI-2pY2JB3FmtS6_wc8B1NT_LhJPjND_eBpnWK3u8zmcXEfGfC-eies-9WMTnRiOgDpfuNnWh-aHaqMeGwrNuGB92iFd2fchSP0Yselew1ZY3xjmcLvvsnWaIx";

// ============================================================ //

bool FcmSender::startup()
{
    curl_global_init(CURL_GLOBAL_SSL);
}

void FcmSender::cleanup()
{
    curl_global_cleanup();
}

bool FcmSender::sendMessage(const std::string &token, uint32_t type, uint32_t numElements)
{
    CURL *curl = curl_easy_init();

    if (!curl) {

        return false;
    }

    // header

    struct curl_slist *header = nullptr;

    std::stringstream key;
    key << "Authorization: key=" << fcmServerKey;

    header = curl_slist_append(header, "Transfer-Encoding: chunked");
    header = curl_slist_append(header, "Content-Type: application/json");
    header = curl_slist_append(header, key.str().c_str());

    // content

    std::stringstream data;

    data << "{\"to\":\"" << token << "\",\"priority\":\"normal\",\"data\":{\"type\":\"" << type << "\",\"numElements\":\"" << numElements << "\"}}";

    std::string dataStr = data.str();

    struct WriteThis pooh;
    pooh.readptr = &dataStr[0];
    pooh.sizeLeft = dataStr.size();

    curl_easy_setopt(curl, CURLOPT_URL, fcmUrl);
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
    curl_easy_setopt(curl, CURLOPT_READDATA, &pooh);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, readCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);

    CURLcode ret = curl_easy_perform(curl);

    curl_slist_free_all(header);

    curl_easy_cleanup(curl);

    if (ret != CURLE_OK) {

        return false;
    }

    //LOG_INFO << "send fcm message ret=" << ret;

    return true;
}

size_t FcmSender::readCallback(void *ptr, size_t size, size_t nmemb, void *userp)
{
    struct WriteThis *pooh = (struct WriteThis*)userp;

    if (size*nmemb < 1) {
        return 0;
    }

    if (pooh->sizeLeft) {
        *(char*)ptr = pooh->readptr[0];
        pooh->readptr++;
        pooh->sizeLeft--;
        return 1;
    }

    return 0;
}

size_t FcmSender::writeCallback(void *ptr, size_t size, size_t nmemb, void *userp)
{
    return size * nmemb;
}

// ============================================================ //

