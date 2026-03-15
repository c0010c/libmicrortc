#include "ports/posix/webrtc_pal_posix.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>

typedef struct webrtc_pal_posix_socket {
    int fd;
} webrtc_pal_posix_socket_t;

static void* posix_mem_malloc(void* user_data, size_t size)
{
    (void) user_data;
    return malloc(size);
}

static void* posix_mem_calloc(void* user_data, size_t count, size_t size)
{
    (void) user_data;
    return calloc(count, size);
}

static void posix_mem_free(void* user_data, void* ptr)
{
    (void) user_data;
    free(ptr);
}

static uint64_t posix_time_monotonic_ms(void* user_data)
{
    struct timespec ts;

    (void) user_data;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }

    return (uint64_t) ts.tv_sec * (uint64_t) 1000 + (uint64_t) (ts.tv_nsec / 1000000L);
}

static webrtc_status_t posix_lock_create(void* user_data, void** out_lock)
{
    pthread_mutex_t* mutex = NULL;

    (void) user_data;

    if (out_lock == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    *out_lock = NULL;
    mutex = (pthread_mutex_t*) calloc(1, sizeof(*mutex));
    if (mutex == NULL) {
        return WEBRTC_STATUS_NO_MEMORY;
    }

    if (pthread_mutex_init(mutex, NULL) != 0) {
        free(mutex);
        return WEBRTC_STATUS_INVALID_STATE;
    }

    *out_lock = mutex;
    return WEBRTC_STATUS_OK;
}

static void posix_lock_destroy(void* user_data, void* lock_handle)
{
    pthread_mutex_t* mutex = (pthread_mutex_t*) lock_handle;

    (void) user_data;

    if (mutex == NULL) {
        return;
    }

    (void) pthread_mutex_destroy(mutex);
    free(mutex);
}

static webrtc_status_t posix_lock_acquire(void* user_data, void* lock_handle)
{
    pthread_mutex_t* mutex = (pthread_mutex_t*) lock_handle;

    (void) user_data;

    if (mutex == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    if (pthread_mutex_lock(mutex) != 0) {
        return WEBRTC_STATUS_INVALID_STATE;
    }

    return WEBRTC_STATUS_OK;
}

static webrtc_status_t posix_lock_release(void* user_data, void* lock_handle)
{
    pthread_mutex_t* mutex = (pthread_mutex_t*) lock_handle;

    (void) user_data;

    if (mutex == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    if (pthread_mutex_unlock(mutex) != 0) {
        return WEBRTC_STATUS_INVALID_STATE;
    }

    return WEBRTC_STATUS_OK;
}

static webrtc_status_t posix_to_sockaddr(const webrtc_pal_sockaddr_t* src, struct sockaddr_in* dst)
{
    if (src == NULL || dst == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    if (src->family != WEBRTC_PAL_IP_FAMILY_V4) {
        return WEBRTC_STATUS_NOT_SUPPORTED;
    }

    memset(dst, 0, sizeof(*dst));
    dst->sin_family = AF_INET;
    dst->sin_port = htons(src->port);
    memcpy(&dst->sin_addr.s_addr, src->addr, 4);

    return WEBRTC_STATUS_OK;
}

static void posix_from_sockaddr(const struct sockaddr_in* src, webrtc_pal_sockaddr_t* dst)
{
    if (src == NULL || dst == NULL) {
        return;
    }

    dst->family = WEBRTC_PAL_IP_FAMILY_V4;
    dst->port = ntohs(src->sin_port);
    memset(dst->addr, 0, sizeof(dst->addr));
    memcpy(dst->addr, &src->sin_addr.s_addr, 4);
}

static webrtc_status_t posix_socket_open_udp(void* user_data, webrtc_pal_socket_t* out_socket)
{
    int fd = -1;
    webrtc_pal_posix_socket_t* handle = NULL;

    (void) user_data;

    if (out_socket == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    *out_socket = NULL;

    handle = (webrtc_pal_posix_socket_t*) calloc(1, sizeof(*handle));
    if (handle == NULL) {
        return WEBRTC_STATUS_NO_MEMORY;
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        free(handle);
        return WEBRTC_STATUS_INVALID_STATE;
    }

    handle->fd = fd;
    *out_socket = (webrtc_pal_socket_t) handle;
    return WEBRTC_STATUS_OK;
}

static webrtc_status_t posix_socket_bind(void* user_data, webrtc_pal_socket_t socket_handle,
                                         const webrtc_pal_sockaddr_t* local_addr)
{
    webrtc_pal_posix_socket_t* handle = (webrtc_pal_posix_socket_t*) socket_handle;
    struct sockaddr_in addr;
    webrtc_status_t status = WEBRTC_STATUS_OK;

    (void) user_data;

    if (handle == NULL || local_addr == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    status = posix_to_sockaddr(local_addr, &addr);
    if (status != WEBRTC_STATUS_OK) {
        return status;
    }

    if (bind(handle->fd, (const struct sockaddr*) &addr, sizeof(addr)) != 0) {
        return WEBRTC_STATUS_INVALID_STATE;
    }

    return WEBRTC_STATUS_OK;
}

static webrtc_status_t posix_socket_sendto(void* user_data, webrtc_pal_socket_t socket_handle,
                                           const webrtc_pal_sockaddr_t* remote_addr, const uint8_t* data, size_t data_len,
                                           size_t* out_sent)
{
    webrtc_pal_posix_socket_t* handle = (webrtc_pal_posix_socket_t*) socket_handle;
    struct sockaddr_in addr;
    webrtc_status_t status = WEBRTC_STATUS_OK;
    ssize_t sent = 0;

    (void) user_data;

    if (handle == NULL || remote_addr == NULL || data == NULL || data_len == 0U) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    status = posix_to_sockaddr(remote_addr, &addr);
    if (status != WEBRTC_STATUS_OK) {
        return status;
    }

    sent = sendto(handle->fd, data, data_len, 0, (const struct sockaddr*) &addr, sizeof(addr));
    if (sent < 0) {
        return WEBRTC_STATUS_INVALID_STATE;
    }

    if (out_sent != NULL) {
        *out_sent = (size_t) sent;
    }

    return WEBRTC_STATUS_OK;
}

static webrtc_status_t posix_socket_recvfrom(void* user_data, webrtc_pal_socket_t socket_handle,
                                             webrtc_pal_sockaddr_t* out_remote_addr, uint8_t* buffer,
                                             size_t buffer_capacity, size_t* out_received)
{
    webrtc_pal_posix_socket_t* handle = (webrtc_pal_posix_socket_t*) socket_handle;
    struct sockaddr_in remote_addr;
    socklen_t remote_len = (socklen_t) sizeof(remote_addr);
    ssize_t recv_len = 0;

    (void) user_data;

    if (handle == NULL || out_remote_addr == NULL || buffer == NULL || out_received == NULL || buffer_capacity == 0U) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    recv_len = recvfrom(handle->fd, buffer, buffer_capacity, 0, (struct sockaddr*) &remote_addr, &remote_len);
    if (recv_len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return WEBRTC_STATUS_INVALID_STATE;
        }

        return WEBRTC_STATUS_INVALID_STATE;
    }

    posix_from_sockaddr(&remote_addr, out_remote_addr);
    *out_received = (size_t) recv_len;
    return WEBRTC_STATUS_OK;
}

static webrtc_status_t posix_socket_set_nonblocking(void* user_data, webrtc_pal_socket_t socket_handle, bool nonblocking)
{
    webrtc_pal_posix_socket_t* handle = (webrtc_pal_posix_socket_t*) socket_handle;
    int flags = 0;

    (void) user_data;

    if (handle == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    flags = fcntl(handle->fd, F_GETFL, 0);
    if (flags < 0) {
        return WEBRTC_STATUS_INVALID_STATE;
    }

    if (nonblocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (fcntl(handle->fd, F_SETFL, flags) != 0) {
        return WEBRTC_STATUS_INVALID_STATE;
    }

    return WEBRTC_STATUS_OK;
}

static void posix_socket_close(void* user_data, webrtc_pal_socket_t socket_handle)
{
    webrtc_pal_posix_socket_t* handle = (webrtc_pal_posix_socket_t*) socket_handle;

    (void) user_data;

    if (handle == NULL) {
        return;
    }

    if (handle->fd >= 0) {
        (void) close(handle->fd);
        handle->fd = -1;
    }

    free(handle);
}

static webrtc_status_t posix_random_fill(void* user_data, uint8_t* out_buffer, size_t buffer_len)
{
    size_t total = 0;

    (void) user_data;

    if (out_buffer == NULL || buffer_len == 0U) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    while (total < buffer_len) {
        ssize_t rc = getrandom(out_buffer + total, buffer_len - total, 0);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }

            return WEBRTC_STATUS_INVALID_STATE;
        }

        total += (size_t) rc;
    }

    return WEBRTC_STATUS_OK;
}

webrtc_status_t webrtc_pal_posix_vtable(webrtc_pal_vtable_t* out_pal)
{
    if (out_pal == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    memset(out_pal, 0, sizeof(*out_pal));

    out_pal->mem_malloc = posix_mem_malloc;
    out_pal->mem_calloc = posix_mem_calloc;
    out_pal->mem_free = posix_mem_free;

    out_pal->time_monotonic_ms = posix_time_monotonic_ms;

    out_pal->lock_create = posix_lock_create;
    out_pal->lock_destroy = posix_lock_destroy;
    out_pal->lock_acquire = posix_lock_acquire;
    out_pal->lock_release = posix_lock_release;

    out_pal->socket_open_udp = posix_socket_open_udp;
    out_pal->socket_bind = posix_socket_bind;
    out_pal->socket_sendto = posix_socket_sendto;
    out_pal->socket_recvfrom = posix_socket_recvfrom;
    out_pal->socket_set_nonblocking = posix_socket_set_nonblocking;
    out_pal->socket_close = posix_socket_close;

    out_pal->random_fill = posix_random_fill;
    out_pal->user_data = NULL;

    return WEBRTC_STATUS_OK;
}
