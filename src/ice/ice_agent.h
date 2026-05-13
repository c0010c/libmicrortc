#ifndef MRTC_ICE_AGENT_H
#define MRTC_ICE_AGENT_H

#include <micrortc/micrortc.h>

typedef struct MRTC_ICE_CANDIDATE {
    char foundation[16];
    unsigned int component;
    char protocol[8];
    unsigned int priority;
    char ip[64];
    unsigned short port;
    char type[16];
} MRTC_ICE_CANDIDATE;

typedef struct MRTC_ICE_HOST_ENDPOINT {
    int fd;
    char ip[64];
    unsigned short port;
    char selected_remote_ip[64];
    unsigned short selected_remote_port;
    int selected_pair_ready;
} MRTC_ICE_HOST_ENDPOINT;

MRTC_STATUS mrtc_ice_parse_candidate(const char *candidate, MRTC_ICE_CANDIDATE *parsed);
void mrtc_ice_host_endpoint_init(MRTC_ICE_HOST_ENDPOINT *endpoint);
MRTC_STATUS mrtc_ice_host_endpoint_bind(MRTC_ICE_HOST_ENDPOINT *endpoint);
void mrtc_ice_host_endpoint_close(MRTC_ICE_HOST_ENDPOINT *endpoint);
MRTC_STATUS mrtc_ice_format_host_endpoint_candidate(const MRTC_ICE_HOST_ENDPOINT *endpoint,
                                                    char *buffer,
                                                    size_t buffer_len,
                                                    size_t *required_len);
MRTC_STATUS mrtc_ice_format_host_candidate(char *buffer, size_t buffer_len, size_t *required_len);
MRTC_STATUS mrtc_ice_format_candidate(const char *type,
                                      const char *ip,
                                      unsigned short port,
                                      char *buffer,
                                      size_t buffer_len,
                                      size_t *required_len);
MRTC_STATUS mrtc_ice_host_endpoint_poll(MRTC_ICE_HOST_ENDPOINT *endpoint,
                                        const char *local_ufrag,
                                        const char *local_pwd,
                                        int timeout_ms,
                                        unsigned char *non_stun_packet,
                                        size_t non_stun_packet_capacity,
                                        size_t *non_stun_packet_len);
int mrtc_ice_packet_is_stun(const unsigned char *packet, size_t packet_len);

#endif /* MRTC_ICE_AGENT_H */
