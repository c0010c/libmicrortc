#include <stddef.h>
#include <stdint.h>

#include "ports/posix/webrtc_pal_posix.h"
#include "webrtc/webrtc_pal.h"
#include "webrtc/webrtc_status.h"

static webrtc_pal_sockaddr_t loopback_addr(uint16_t port)
{
    webrtc_pal_sockaddr_t addr;

    addr.family = WEBRTC_PAL_IP_FAMILY_V4;
    addr.port = port;
    addr.addr[0] = 127;
    addr.addr[1] = 0;
    addr.addr[2] = 0;
    addr.addr[3] = 1;

    return addr;
}

static webrtc_status_t bind_with_retry(const webrtc_pal_vtable_t* pal,
                                       webrtc_pal_socket_t socket_handle,
                                       uint16_t* out_port)
{
    webrtc_status_t status = WEBRTC_STATUS_OK;
    uint16_t port = 36000;
    uint32_t attempts = 0;

    for (attempts = 0; attempts < 256; ++attempts) {
        webrtc_pal_sockaddr_t addr = loopback_addr((uint16_t) (port + attempts));
        status = pal->socket_bind(pal->user_data, socket_handle, &addr);
        if (status == WEBRTC_STATUS_OK) {
            *out_port = addr.port;
            return WEBRTC_STATUS_OK;
        }
    }

    return status;
}

int main(void)
{
    webrtc_pal_vtable_t pal;
    webrtc_status_t status = WEBRTC_STATUS_OK;
    webrtc_pal_socket_t socket_a = NULL;
    webrtc_pal_socket_t socket_b = NULL;
    uint16_t port_a = 0;
    uint16_t port_b = 0;
    const uint8_t outbound[] = {0x10, 0x20, 0x30, 0x40};
    const uint8_t outbound2[] = {0x01, 0x02};
    uint8_t inbound[32];
    size_t sent = 0;
    size_t recv_len = 0;
    webrtc_pal_sockaddr_t peer;
    uint8_t random_bytes[16];
    uint64_t t0 = 0;
    uint64_t t1 = 0;

    status = webrtc_pal_posix_vtable(&pal);
    if (status != WEBRTC_STATUS_OK) {
        return 1;
    }

    status = pal.socket_open_udp(pal.user_data, &socket_a);
    if (status != WEBRTC_STATUS_OK || socket_a == NULL) {
        return 2;
    }

    status = pal.socket_open_udp(pal.user_data, &socket_b);
    if (status != WEBRTC_STATUS_OK || socket_b == NULL) {
        pal.socket_close(pal.user_data, socket_a);
        return 3;
    }

    status = bind_with_retry(&pal, socket_a, &port_a);
    if (status != WEBRTC_STATUS_OK) {
        pal.socket_close(pal.user_data, socket_b);
        pal.socket_close(pal.user_data, socket_a);
        return 4;
    }

    status = bind_with_retry(&pal, socket_b, &port_b);
    if (status != WEBRTC_STATUS_OK || port_b == port_a) {
        pal.socket_close(pal.user_data, socket_b);
        pal.socket_close(pal.user_data, socket_a);
        return 5;
    }

    status = pal.socket_set_nonblocking(pal.user_data, socket_a, 0);
    if (status != WEBRTC_STATUS_OK) {
        pal.socket_close(pal.user_data, socket_b);
        pal.socket_close(pal.user_data, socket_a);
        return 6;
    }

    status = pal.socket_set_nonblocking(pal.user_data, socket_b, 0);
    if (status != WEBRTC_STATUS_OK) {
        pal.socket_close(pal.user_data, socket_b);
        pal.socket_close(pal.user_data, socket_a);
        return 7;
    }

    {
        webrtc_pal_sockaddr_t target = loopback_addr(port_b);
        status = pal.socket_sendto(pal.user_data, socket_a, &target, outbound, sizeof(outbound), &sent);
    }
    if (status != WEBRTC_STATUS_OK || sent != sizeof(outbound)) {
        pal.socket_close(pal.user_data, socket_b);
        pal.socket_close(pal.user_data, socket_a);
        return 8;
    }

    recv_len = sizeof(inbound);
    status = pal.socket_recvfrom(pal.user_data, socket_b, &peer, inbound, sizeof(inbound), &recv_len);
    if (status != WEBRTC_STATUS_OK || recv_len != sizeof(outbound)) {
        pal.socket_close(pal.user_data, socket_b);
        pal.socket_close(pal.user_data, socket_a);
        return 9;
    }

    if (peer.port != port_a) {
        pal.socket_close(pal.user_data, socket_b);
        pal.socket_close(pal.user_data, socket_a);
        return 10;
    }

    status = pal.socket_sendto(pal.user_data, socket_b, &peer, outbound2, sizeof(outbound2), &sent);
    if (status != WEBRTC_STATUS_OK || sent != sizeof(outbound2)) {
        pal.socket_close(pal.user_data, socket_b);
        pal.socket_close(pal.user_data, socket_a);
        return 11;
    }

    recv_len = sizeof(inbound);
    status = pal.socket_recvfrom(pal.user_data, socket_a, &peer, inbound, sizeof(inbound), &recv_len);
    if (status != WEBRTC_STATUS_OK || recv_len != sizeof(outbound2)) {
        pal.socket_close(pal.user_data, socket_b);
        pal.socket_close(pal.user_data, socket_a);
        return 12;
    }

    t0 = pal.time_monotonic_ms(pal.user_data);
    t1 = pal.time_monotonic_ms(pal.user_data);
    if (t1 < t0) {
        pal.socket_close(pal.user_data, socket_b);
        pal.socket_close(pal.user_data, socket_a);
        return 13;
    }

    status = pal.random_fill(pal.user_data, random_bytes, sizeof(random_bytes));
    if (status != WEBRTC_STATUS_OK) {
        pal.socket_close(pal.user_data, socket_b);
        pal.socket_close(pal.user_data, socket_a);
        return 14;
    }

    pal.socket_close(pal.user_data, socket_b);
    pal.socket_close(pal.user_data, socket_a);

    return 0;
}
