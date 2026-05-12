#ifndef MRTC_TURN_CLIENT_H
#define MRTC_TURN_CLIENT_H

#include <micrortc/micrortc.h>

typedef struct MRTC_ICE_SERVER_URL {
    char scheme[8];
    char host[256];
    unsigned short port;
    char transport[8];
} MRTC_ICE_SERVER_URL;

MRTC_STATUS mrtc_ice_server_url_parse(const char *url, MRTC_ICE_SERVER_URL *parsed);

#endif /* MRTC_TURN_CLIENT_H */
