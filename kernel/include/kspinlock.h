#ifndef __KSPINLOCK_H__
#define __KSPINLOCK_H__

typedef struct kspinlock {
    int locked;
}kspinlock_t;


void init_klock(kspinlock_t* lk, char* name);
void kspin_lock(kspinlock_t* lk);
void kspin_unlock(kspinlock_t* lk);

#endif