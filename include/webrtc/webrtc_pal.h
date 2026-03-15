#ifndef WEBRTC_WEBRTC_PAL_H
#define WEBRTC_WEBRTC_PAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "webrtc/webrtc_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* webrtc_pal_socket_t;

typedef enum webrtc_pal_ip_family {
    WEBRTC_PAL_IP_FAMILY_V4 = 4,
    WEBRTC_PAL_IP_FAMILY_V6 = 6
} webrtc_pal_ip_family_t;

typedef struct webrtc_pal_sockaddr {
    uint8_t family;
    uint16_t port;
    uint8_t addr[16];
} webrtc_pal_sockaddr_t;

typedef void* (*webrtc_pal_malloc_fn)(void* user_data, size_t size);
typedef void* (*webrtc_pal_calloc_fn)(void* user_data, size_t count, size_t size);
typedef void (*webrtc_pal_free_fn)(void* user_data, void* ptr);

typedef uint64_t (*webrtc_pal_time_monotonic_ms_fn)(void* user_data);

typedef webrtc_status_t (*webrtc_pal_lock_create_fn)(void* user_data, void** out_lock);
typedef void (*webrtc_pal_lock_destroy_fn)(void* user_data, void* lock_handle);
typedef webrtc_status_t (*webrtc_pal_lock_acquire_fn)(void* user_data, void* lock_handle);
typedef webrtc_status_t (*webrtc_pal_lock_release_fn)(void* user_data, void* lock_handle);

typedef webrtc_status_t (*webrtc_pal_socket_open_udp_fn)(void* user_data, webrtc_pal_socket_t* out_socket);
typedef webrtc_status_t (*webrtc_pal_socket_bind_fn)(void* user_data, webrtc_pal_socket_t socket_handle,
                                                      const webrtc_pal_sockaddr_t* local_addr);
typedef webrtc_status_t (*webrtc_pal_socket_sendto_fn)(void* user_data, webrtc_pal_socket_t socket_handle,
                                                        const webrtc_pal_sockaddr_t* remote_addr, const uint8_t* data,
                                                        size_t data_len, size_t* out_sent);
typedef webrtc_status_t (*webrtc_pal_socket_recvfrom_fn)(void* user_data, webrtc_pal_socket_t socket_handle,
                                                          webrtc_pal_sockaddr_t* out_remote_addr, uint8_t* buffer,
                                                          size_t buffer_capacity, size_t* out_received);
typedef webrtc_status_t (*webrtc_pal_socket_set_nonblocking_fn)(void* user_data, webrtc_pal_socket_t socket_handle,
                                                                 bool nonblocking);
typedef void (*webrtc_pal_socket_close_fn)(void* user_data, webrtc_pal_socket_t socket_handle);

typedef webrtc_status_t (*webrtc_pal_random_fill_fn)(void* user_data, uint8_t* out_buffer, size_t buffer_len);

typedef struct webrtc_pal_vtable {
    webrtc_pal_malloc_fn mem_malloc;
    webrtc_pal_calloc_fn mem_calloc;
    webrtc_pal_free_fn mem_free;

    webrtc_pal_time_monotonic_ms_fn time_monotonic_ms;

    webrtc_pal_lock_create_fn lock_create;
    webrtc_pal_lock_destroy_fn lock_destroy;
    webrtc_pal_lock_acquire_fn lock_acquire;
    webrtc_pal_lock_release_fn lock_release;

    webrtc_pal_socket_open_udp_fn socket_open_udp;
    webrtc_pal_socket_bind_fn socket_bind;
    webrtc_pal_socket_sendto_fn socket_sendto;
    webrtc_pal_socket_recvfrom_fn socket_recvfrom;
    webrtc_pal_socket_set_nonblocking_fn socket_set_nonblocking;
    webrtc_pal_socket_close_fn socket_close;

    webrtc_pal_random_fill_fn random_fill;

    void* user_data;
} webrtc_pal_vtable_t;

#ifdef __cplusplus
}
#endif

#endif /* WEBRTC_WEBRTC_PAL_H */
