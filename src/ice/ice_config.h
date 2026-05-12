#ifndef MRTC_ICE_CONFIG_H
#define MRTC_ICE_CONFIG_H

#include <micrortc/peer_connection.h>

#include <stddef.h>

#define MRTC_ICE_CONFIG_MAX_SERVERS 8u

typedef struct MRTC_ICE_CONFIG {
    MRTC_ICE_SERVER servers[MRTC_ICE_CONFIG_MAX_SERVERS];
    size_t server_count;
} MRTC_ICE_CONFIG;

MRTC_STATUS mrtc_ice_config_load(const char *path, MRTC_ICE_CONFIG *config);
void mrtc_ice_config_deinit(MRTC_ICE_CONFIG *config);

#endif /* MRTC_ICE_CONFIG_H */
