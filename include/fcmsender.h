
#ifndef SERVER_FCM_SENDER_H_
#define SERVER_FCM_SENDER_H_

#include <curl/curl.h>

#include <string>

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

#endif
