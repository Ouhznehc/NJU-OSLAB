#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

typedef struct spinlock{
    const char *name;
    int locked;
    int cpu;
}spinlock_t;


void init_lock(spinlock_t *lk, char *name);
void spin_lock(spinlock_t *lk);
void spin_unlock(spinlock_t *lk);

#endif