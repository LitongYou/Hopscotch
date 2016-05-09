#ifndef __SYNC_H_
#define __SYNC_H_

#include <pthread.h>

typedef pthread_mutex_t lock_t;

#define LOCK_INIT(lock) pthread_mutex_init(lock, NULL);
#define LOCK_ACQUIRE(lock) pthread_mutex_lock(lock);
#define LOCK_RELEASE(lock) pthread_mutex_unlock(lock);
#define LOCK_DISPOSE(lock) pthread_mutex_destroy(lock);

#endif
