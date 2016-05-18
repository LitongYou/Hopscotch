#ifndef __SYNC_H_
#define __SYNC_H_

#include <pthread.h>

#ifdef MUTEX
typedef pthread_mutex_t lock_t;

#define LOCK_INIT(lock) pthread_mutex_init(lock, NULL);
#define LOCK_ACQUIRE(lock) pthread_mutex_lock(lock);
#define LOCK_RELEASE(lock) pthread_mutex_unlock(lock);
#define LOCK_DISPOSE(lock) pthread_mutex_destroy(lock);
#endif

#ifdef SPINLOCK
typedef pthread_spinlock_t lock_t;
#define LOCK_INIT(lock) pthread_spin_init(lock, 0);
#define LOCK_ACQUIRE(lock) pthread_spin_lock(lock);
#define LOCK_RELEASE(lock) pthread_spin_unlock(lock);
#define LOCK_DISPOSE(lock) pthread_spin_destroy(lock);
#endif

#endif
