
#include "fcmsender.h"
#include "logger.h"

#include <sstream>

const char *FcmSender::fcmUrl = "https://fcm.googleapis.com/fcm/send";
const char *FcmSender::fcmServerKey = "AAAAPYes6ds:APA91bEy1dPjayq9vOzI-2pY2JB3FmtS6_wc8B1NT_LhJPjND_eBpnWK3u8zmcXEfGfC-eies-9WMTnRiOgDpfuNnWh-aHaqMeGwrNuGB92iFd2fchSP0Yselew1ZY3xjmcLvvsnWaIx";

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
