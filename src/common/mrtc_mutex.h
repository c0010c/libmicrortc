#ifndef MRTC_MUTEX_H
#define MRTC_MUTEX_H

#include <micrortc/micrortc.h>

#include <pthread.h>

typedef struct MRTC_MUTEX {
    pthread_mutex_t native;
    int initialized;
} MRTC_MUTEX;

MRTC_STATUS mrtc_mutex_init(MRTC_MUTEX *mutex);
void mrtc_mutex_destroy(MRTC_MUTEX *mutex);
MRTC_STATUS mrtc_mutex_lock(MRTC_MUTEX *mutex);
void mrtc_mutex_unlock(MRTC_MUTEX *mutex);

#endif /* MRTC_MUTEX_H */
