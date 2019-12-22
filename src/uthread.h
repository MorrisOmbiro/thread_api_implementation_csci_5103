#ifndef OS_2_UTHREAD_H
#define OS_2_UTHREAD_H
#include <sys/time.h>
#include <queue>
#define STACK_SIZE (2 * 1024 * 1024)

struct lock_node {
  int tid;
  lock_node* next;
};

typedef struct lock_t {
  int holding_tid;
  lock_node* head;
  lock_node* tail;
} lock_t;

int uthread_create(void * (*start_routine)(void *), void *arg);
int uthread_yield(void);
int uthread_self(void);
int uthread_join(int tid, void**retval);

int uthread_init(int time_slice);
int uthread_terminate(int tid);
int uthread_suspend(int tid);
int uthread_resume(int tid);

int lock_init(lock_t*);
int acquire(lock_t*);
int release(lock_t*);

#endif //OS_2_UTHREAD_H
