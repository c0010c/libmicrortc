#include "mrtc_mutex.h"

MRTC_STATUS mrtc_mutex_init(MRTC_MUTEX *mutex)
{
    if (mutex == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    if (pthread_mutex_init(&mutex->native, 0) != 0) {
        mutex->initialized = 0;
        return MRTC_STATUS_INVALID_STATE;
    }

    mutex->initialized = 1;
    return MRTC_STATUS_OK;
}

void mrtc_mutex_destroy(MRTC_MUTEX *mutex)
{
    if (mutex == 0 || !mutex->initialized) {
        return;
    }

    pthread_mutex_destroy(&mutex->native);
    mutex->initialized = 0;
}

MRTC_STATUS mrtc_mutex_lock(MRTC_MUTEX *mutex)
{
    if (mutex == 0 || !mutex->initialized) {
        return MRTC_STATUS_INVALID_ARG;
    }

    return pthread_mutex_lock(&mutex->native) == 0 ? MRTC_STATUS_OK : MRTC_STATUS_INVALID_STATE;
}

void mrtc_mutex_unlock(MRTC_MUTEX *mutex)
{
    if (mutex != 0 && mutex->initialized) {
        pthread_mutex_unlock(&mutex->native);
    }
}
