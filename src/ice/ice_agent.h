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

MRTC_STATUS mrtc_ice_parse_candidate(const char *candidate, MRTC_ICE_CANDIDATE *parsed);
MRTC_STATUS mrtc_ice_format_host_candidate(char *buffer, size_t buffer_len, size_t *required_len);
MRTC_STATUS mrtc_ice_format_candidate(const char *type,
                                      const char *ip,
                                      unsigned short port,
                                      char *buffer,
                                      size_t buffer_len,
                                      size_t *required_len);
int mrtc_ice_packet_is_stun(const unsigned char *packet, size_t packet_len);

#endif /* MRTC_ICE_AGENT_H */
