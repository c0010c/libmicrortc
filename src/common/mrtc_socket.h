#ifndef MRTC_SOCKET_H
#define MRTC_SOCKET_H

#include <micrortc/micrortc.h>

#include <netinet/in.h>
#include <stddef.h>

typedef struct MRTC_SOCKET_ADDRESS {
    struct sockaddr_in addr;
    socklen_t len;
} MRTC_SOCKET_ADDRESS;

MRTC_STATUS mrtc_socket_address_from_ipv4(const char *ip, unsigned short port, MRTC_SOCKET_ADDRESS *address);
MRTC_STATUS mrtc_socket_address_to_string(const MRTC_SOCKET_ADDRESS *address,
                                          char *buffer,
                                          size_t buffer_len);
unsigned short mrtc_socket_address_port(const MRTC_SOCKET_ADDRESS *address);

#endif /* MRTC_SOCKET_H */
