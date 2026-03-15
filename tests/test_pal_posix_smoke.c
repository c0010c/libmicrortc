#include <stddef.h>
#include <stdint.h>

#include "ports/posix/webrtc_pal_posix.h"
#include "webrtc/webrtc_pal.h"
#include "webrtc/webrtc_status.h"

static int check_not_all_zero(const uint8_t* data, size_t len)
{
    size_t i = 0;

    for (i = 0; i < len; ++i) {
        if (data[i] != 0U) {
            return 1;
        }
    }

    return 0;
}

int main(void)
{
    webrtc_pal_vtable_t pal;
    webrtc_status_t status = WEBRTC_STATUS_OK;
    void* lock_handle = NULL;
    uint8_t random_bytes[32];
    uint64_t t0 = 0;
    uint64_t t1 = 0;
    webrtc_pal_socket_t socket_handle = NULL;
    webrtc_pal_sockaddr_t local_addr;

    status = webrtc_pal_posix_vtable(&pal);
    if (status != WEBRTC_STATUS_OK) {
        return 1;
    }

    if (pal.mem_malloc == NULL || pal.mem_calloc == NULL || pal.mem_free == NULL ||
        pal.time_monotonic_ms == NULL || pal.lock_create == NULL || pal.lock_destroy == NULL ||
        pal.lock_acquire == NULL || pal.lock_release == NULL || pal.socket_open_udp == NULL ||
        pal.socket_bind == NULL || pal.socket_sendto == NULL || pal.socket_recvfrom == NULL ||
        pal.socket_set_nonblocking == NULL || pal.socket_close == NULL || pal.random_fill == NULL) {
        return 2;
    }

    t0 = pal.time_monotonic_ms(pal.user_data);
    t1 = pal.time_monotonic_ms(pal.user_data);
    if (t1 < t0) {
        return 3;
    }

    status = pal.random_fill(pal.user_data, random_bytes, sizeof(random_bytes));
    if (status != WEBRTC_STATUS_OK) {
        return 4;
    }

    if (!check_not_all_zero(random_bytes, sizeof(random_bytes))) {
        return 5;
    }

    status = pal.lock_create(pal.user_data, &lock_handle);
    if (status != WEBRTC_STATUS_OK || lock_handle == NULL) {
        return 6;
    }

    status = pal.lock_acquire(pal.user_data, lock_handle);
    if (status != WEBRTC_STATUS_OK) {
        pal.lock_destroy(pal.user_data, lock_handle);
        return 7;
    }

    status = pal.lock_release(pal.user_data, lock_handle);
    if (status != WEBRTC_STATUS_OK) {
        pal.lock_destroy(pal.user_data, lock_handle);
        return 8;
    }

    pal.lock_destroy(pal.user_data, lock_handle);

    status = pal.socket_open_udp(pal.user_data, &socket_handle);
    if (status != WEBRTC_STATUS_OK || socket_handle == NULL) {
        return 9;
    }

    local_addr.family = WEBRTC_PAL_IP_FAMILY_V4;
    local_addr.port = 0;
    local_addr.addr[0] = 127;
    local_addr.addr[1] = 0;
    local_addr.addr[2] = 0;
    local_addr.addr[3] = 1;

    status = pal.socket_bind(pal.user_data, socket_handle, &local_addr);
    if (status != WEBRTC_STATUS_OK) {
        pal.socket_close(pal.user_data, socket_handle);
        return 10;
    }

    pal.socket_close(pal.user_data, socket_handle);
    return 0;
}
