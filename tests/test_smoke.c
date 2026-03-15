#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "webrtc/webrtc.h"

static void* test_mem_malloc(void* user_data, size_t size)
{
    (void) user_data;
    return malloc(size);
}

static void* test_mem_calloc(void* user_data, size_t count, size_t size)
{
    (void) user_data;
    return calloc(count, size);
}

static void test_mem_free(void* user_data, void* ptr)
{
    (void) user_data;
    free(ptr);
}

static uint64_t test_time_monotonic_ms(void* user_data)
{
    (void) user_data;
    return (uint64_t) time(NULL) * (uint64_t) 1000;
}

static webrtc_status_t test_lock_create(void* user_data, void** out_lock)
{
    (void) user_data;
    if (out_lock == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    *out_lock = (void*) 0x1;
    return WEBRTC_STATUS_OK;
}

static void test_lock_destroy(void* user_data, void* lock_handle)
{
    (void) user_data;
    (void) lock_handle;
}

static webrtc_status_t test_lock_acquire(void* user_data, void* lock_handle)
{
    (void) user_data;
    (void) lock_handle;
    return WEBRTC_STATUS_OK;
}

static webrtc_status_t test_lock_release(void* user_data, void* lock_handle)
{
    (void) user_data;
    (void) lock_handle;
    return WEBRTC_STATUS_OK;
}

static webrtc_status_t test_socket_open_udp(void* user_data, webrtc_pal_socket_t* out_socket)
{
    (void) user_data;
    if (out_socket == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    *out_socket = (void*) 0x1;
    return WEBRTC_STATUS_OK;
}

static webrtc_status_t test_socket_bind(void* user_data, webrtc_pal_socket_t socket_handle,
                                        const webrtc_pal_sockaddr_t* local_addr)
{
    (void) user_data;
    (void) socket_handle;
    (void) local_addr;
    return WEBRTC_STATUS_OK;
}

static webrtc_status_t test_socket_sendto(void* user_data, webrtc_pal_socket_t socket_handle,
                                          const webrtc_pal_sockaddr_t* remote_addr, const uint8_t* data,
                                          size_t data_len, size_t* out_sent)
{
    (void) user_data;
    (void) socket_handle;
    (void) remote_addr;
    (void) data;
    if (out_sent != NULL) {
        *out_sent = data_len;
    }
    return WEBRTC_STATUS_OK;
}

static webrtc_status_t test_socket_recvfrom(void* user_data, webrtc_pal_socket_t socket_handle,
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
    return WEBRTC_STATUS_OK;
}

static webrtc_status_t test_socket_set_nonblocking(void* user_data, webrtc_pal_socket_t socket_handle, bool nonblocking)
{
    (void) user_data;
    (void) socket_handle;
    (void) nonblocking;
    return WEBRTC_STATUS_OK;
}

static void test_socket_close(void* user_data, webrtc_pal_socket_t socket_handle)
{
    (void) user_data;
    (void) socket_handle;
}

static webrtc_status_t test_random_fill(void* user_data, uint8_t* out_buffer, size_t buffer_len)
{
    size_t i = 0;

    (void) user_data;

    if (out_buffer == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    for (i = 0; i < buffer_len; ++i) {
        out_buffer[i] = (uint8_t) (i + 1U);
    }

    return WEBRTC_STATUS_OK;
}

int main(void)
{
    webrtc_instance_t* instance = NULL;
    webrtc_status_t status = WEBRTC_STATUS_OK;
    webrtc_config_t config;
    webrtc_pal_vtable_t pal;

    status = webrtc_config_init_default(&config);
    if (status != WEBRTC_STATUS_OK) {
        return 1;
    }

    pal.mem_malloc = test_mem_malloc;
    pal.mem_calloc = test_mem_calloc;
    pal.mem_free = test_mem_free;
    pal.time_monotonic_ms = test_time_monotonic_ms;
    pal.lock_create = test_lock_create;
    pal.lock_destroy = test_lock_destroy;
    pal.lock_acquire = test_lock_acquire;
    pal.lock_release = test_lock_release;
    pal.socket_open_udp = test_socket_open_udp;
    pal.socket_bind = test_socket_bind;
    pal.socket_sendto = test_socket_sendto;
    pal.socket_recvfrom = test_socket_recvfrom;
    pal.socket_set_nonblocking = test_socket_set_nonblocking;
    pal.socket_close = test_socket_close;
    pal.random_fill = test_random_fill;
    pal.user_data = NULL;

    status = webrtc_init(&config, &pal, &instance);
    if (status != WEBRTC_STATUS_OK || instance == NULL) {
        return 2;
    }

    status = webrtc_pump_step(instance, (uint64_t) 0, (uint32_t) 0);
    if (status != WEBRTC_STATUS_OK) {
        webrtc_deinit(instance);
        return 3;
    }

    status = webrtc_pump_step(instance, (uint64_t) 0, (uint32_t) 1);
    if (status != WEBRTC_STATUS_OK) {
        webrtc_deinit(instance);
        return 4;
    }

    webrtc_deinit(instance);
    return 0;
}
