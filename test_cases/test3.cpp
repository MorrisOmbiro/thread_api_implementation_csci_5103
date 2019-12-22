#include "uthread.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// test lock 
lock_t l; 
void* lock(void* argv) {
  int tid = uthread_self();
  printf("Starting Thread %d\n", tid);
  acquire(&l);
  printf("Thread %d acquired the lock\n", tid);
  int i = 0;
  while (i < 1000000) i++;
  printf("Thread %d is releasing the lock\n", tid);
  release(&l);
  printf("Thread %d has released the lock\n", tid);

  char* ret = (char*)malloc(1024);
  sprintf(ret, "Data from thread %d", tid);
  return (void*)ret;
}

int main(int argc, char** argv){
  if (argc < 2) {
    fprintf(stderr, "Usage: ./a.out <threads> \n");
    exit(1);
  }
  int thread_count = atoi(argv[1]);
  printf("Thread_count: %d\n", thread_count);
  int *threads = new int[thread_count];

  int ret = uthread_init(1000);
  if (ret != 0) {
    fprintf(stderr, "uthread_init FAIL!\n");
    exit(1);
  }

  lock_init(&l);

  for (int i = 0; i < thread_count; i++) {
    int tid = uthread_create(lock, NULL);
    printf("tid=%d\n", tid);
    threads[i] = tid;
  }

  for (int i = 0; i < thread_count; i++) {
    void* data_back;
    uthread_join(threads[i], &data_back);
    printf("Revieved '%s' from thread at %d\n", (char*)data_back, i);
    free(data_back);
  }

  delete[] threads;
  return 0;
}