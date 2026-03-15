#include "ports/rtos_stub/webrtc_pal_rtos_stub.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void* rtos_stub_mem_malloc(void* user_data, size_t size)
{
    (void) user_data;
    return malloc(size);
}

static void* rtos_stub_mem_calloc(void* user_data, size_t count, size_t size)
{
    (void) user_data;
    return calloc(count, size);
}

static void rtos_stub_mem_free(void* user_data, void* ptr)
{
    (void) user_data;
    free(ptr);
}

static uint64_t rtos_stub_time_monotonic_ms(void* user_data)
{
    (void) user_data;
    return 0;
}

static webrtc_status_t rtos_stub_lock_create(void* user_data, void** out_lock)
{
    (void) user_data;
    (void) out_lock;
    return WEBRTC_STATUS_NOT_IMPLEMENTED;
}

static void rtos_stub_lock_destroy(void* user_data, void* lock_handle)
{
    (void) user_data;
    (void) lock_handle;
}

static webrtc_status_t rtos_stub_lock_acquire(void* user_data, void* lock_handle)
{
    (void) user_data;
    (void) lock_handle;
    return WEBRTC_STATUS_NOT_IMPLEMENTED;
}

static webrtc_status_t rtos_stub_lock_release(void* user_data, void* lock_handle)
{
    (void) user_data;
    (void) lock_handle;
    return WEBRTC_STATUS_NOT_IMPLEMENTED;
}

static webrtc_status_t rtos_stub_socket_open_udp(void* user_data, webrtc_pal_socket_t* out_socket)
{
    (void) user_data;
    if (out_socket != NULL) {
        *out_socket = NULL;
    }
    return WEBRTC_STATUS_NOT_IMPLEMENTED;
}

static webrtc_status_t rtos_stub_socket_bind(void* user_data, webrtc_pal_socket_t socket_handle,
                                             const webrtc_pal_sockaddr_t* local_addr)
{
    (void) user_data;
    (void) socket_handle;
    (void) local_addr;
    return WEBRTC_STATUS_NOT_IMPLEMENTED;
}

static webrtc_status_t rtos_stub_socket_sendto(void* user_data, webrtc_pal_socket_t socket_handle,
                                               const webrtc_pal_sockaddr_t* remote_addr, const uint8_t* data,
                                               size_t data_len, size_t* out_sent)
{
    (void) user_data;
    (void) socket_handle;
    (void) remote_addr;
    (void) data;
    (void) data_len;
    if (out_sent != NULL) {
        *out_sent = 0;
    }
    return WEBRTC_STATUS_NOT_IMPLEMENTED;
}

static webrtc_status_t rtos_stub_socket_recvfrom(void* user_data, webrtc_pal_socket_t socket_handle,
                                                 webrtc_pal_sockaddr_t* out_remote_addr, uint8_t* buffer,
                                                 size_t buffer_capacity, size_t* out_received)
{
    (void) user_data;
    (void) socket_handle;
    (void) out_remote_addr;
    (void) buffer;
    (void) buffer_capacity;
    if (out_received != NULL) {
        *out_received = 0;
    }
    return WEBRTC_STATUS_NOT_IMPLEMENTED;
}

static webrtc_status_t rtos_stub_socket_set_nonblocking(void* user_data, webrtc_pal_socket_t socket_handle, bool nonblocking)
{
    (void) user_data;
    (void) socket_handle;
    (void) nonblocking;
    return WEBRTC_STATUS_NOT_IMPLEMENTED;
}

static void rtos_stub_socket_close(void* user_data, webrtc_pal_socket_t socket_handle)
{
    (void) user_data;
    (void) socket_handle;
}

static webrtc_status_t rtos_stub_random_fill(void* user_data, uint8_t* out_buffer, size_t buffer_len)
{
    (void) user_data;
    (void) out_buffer;
    (void) buffer_len;
    return WEBRTC_STATUS_NOT_IMPLEMENTED;
}

webrtc_status_t webrtc_pal_rtos_stub_vtable(webrtc_pal_vtable_t* out_pal)
{
    if (out_pal == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    memset(out_pal, 0, sizeof(*out_pal));

    out_pal->mem_malloc = rtos_stub_mem_malloc;
    out_pal->mem_calloc = rtos_stub_mem_calloc;
    out_pal->mem_free = rtos_stub_mem_free;

    out_pal->time_monotonic_ms = rtos_stub_time_monotonic_ms;

    out_pal->lock_create = rtos_stub_lock_create;
    out_pal->lock_destroy = rtos_stub_lock_destroy;
    out_pal->lock_acquire = rtos_stub_lock_acquire;
    out_pal->lock_release = rtos_stub_lock_release;

    out_pal->socket_open_udp = rtos_stub_socket_open_udp;
    out_pal->socket_bind = rtos_stub_socket_bind;
    out_pal->socket_sendto = rtos_stub_socket_sendto;
    out_pal->socket_recvfrom = rtos_stub_socket_recvfrom;
    out_pal->socket_set_nonblocking = rtos_stub_socket_set_nonblocking;
    out_pal->socket_close = rtos_stub_socket_close;

    out_pal->random_fill = rtos_stub_random_fill;
    out_pal->user_data = NULL;

    return WEBRTC_STATUS_OK;
}
