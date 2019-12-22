#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <cstring>

#include "uthread.h"
#include "uthread_internal.h"
#include <queue>
#include <list>
#include <map>

using namespace std;

int next_usable_tid = 0; // root thread is tid 0
static sigjmp_buf scheduler_buf;
// map for suspended w/ tid
sigset_t timer; 
// IMPORTANT ----
// ***
map<int, TCB*> TCB_store; // TCBs go on heap to avoid re-allocation errors
// ***
// IMPORTANT ----

// itimer elements 
struct itimerval itime_; 
struct timeval _time; 


TCB* running_tcb; 
queue<TCB*> ready_tcb;
list<TCB*> cleanup_tcb;

class ControlBlocker {
 private:
  sigset_t sigset;
 public:
  ControlBlocker();
  ~ControlBlocker();
};

TCB* find_tcb(const int tid) {
  try {
    return TCB_store.at(tid);
  } catch (out_of_range& e) {
    return NULL;
  }
}

void scheduler(void) { // Scheduler function: cannot return
  sigjmp_buf* buf;

  for(auto it=cleanup_tcb.begin(); it != cleanup_tcb.end(); it++) {
    if (!(*it)->get_critical()) {
      (*it)->terminate();
    }
  }

  if (running_tcb->get_critical()
      && running_tcb->get_state() != SUSPENDED)
  { // critical section
    buf = running_tcb->get_buf();
    running_tcb->set_interrupt();
  } else if (ready_tcb.size() > 0) { // regular swap
    ready_tcb.push(running_tcb);
    running_tcb->set_state(State::READY);

    while(ready_tcb.front()->get_state() != READY) {
      ready_tcb.pop();
    }

    running_tcb = ready_tcb.front();
    ready_tcb.pop();
    buf = running_tcb->get_buf();
    running_tcb->set_state(State::RUNNING);
    sigdelset(&(*buf)->__saved_mask, SIGVTALRM);
    running_tcb->interrupted();
  } else { // Current thread is the only thread
    buf = running_tcb->get_buf();
    sigdelset(&(*buf)->__saved_mask, SIGVTALRM);
    running_tcb->interrupted();
  }

  siglongjmp(*buf, 1);
}

void preempt_thread(int);

int uthread_create(void * (*start_routine)(void *), void *arg) {
  TCB* new_tcb = new TCB(start_routine, arg);
  TCB_store.insert({new_tcb->get_tid(), new_tcb});
  ready_tcb.push(new_tcb);
  return new_tcb->get_tid();
}


int uthread_yield(void) {
  sigjmp_buf buf;
  
  sigprocmask(SIG_BLOCK, &timer, NULL);
  // save the current context
  running_tcb->set_state(State::READY); 

  if (sigsetjmp(*(running_tcb->get_buf()), 1) == 0) {
    siglongjmp(scheduler_buf,1); // to the scheduler's context
  }

  return 0;
}

int uthread_self(void) {
  return running_tcb->get_tid();
}

int uthread_join(int tid, void**retval){
  TCB* tcb = find_tcb(tid);

  while (tcb->get_state() != State::TERMINATED) {
    tcb->set_joining_with(running_tcb->get_tid());
    uthread_suspend(running_tcb->get_tid());
  }

  tcb->populate_ret_ptr(retval);
  delete tcb;
  return 0;
}

// uthread control
    // needs to set up a time interrupt that goes off
    // to call a new uthread_yield()
    // call this to set the length of the timeslice
    // the scheduler runs once every time slice
    // create a stack for the scheduler to run on 
int uthread_init(int time_slice){

    sigemptyset(&timer);
    sigaddset(&timer, SIGVTALRM);
    
    _time.tv_sec = time_slice / 1000000;
    _time.tv_usec = time_slice % 1000000;
    itime_.it_interval = _time; 
    itime_.it_value = _time;
    setitimer(ITIMER_VIRTUAL, &itime_, NULL); // run agains the user-mode CPU time
    // create a stack and jmpbuf for TCB  

    struct sigaction *sa = (struct sigaction*) malloc(sizeof(struct sigaction));
    memset(sa, 0, sizeof(struct sigaction));

    sigset_t vtalrm;
    sigemptyset(&vtalrm);
    sigaddset(&vtalrm, SIGVTALRM);

    sa->sa_handler = preempt_thread;
    sa->sa_mask = vtalrm;
    sigaction(SIGVTALRM, sa, NULL);

    void* scheduler_stack = malloc(4096);
    sigsetjmp(scheduler_buf, 0);
    scheduler_buf->__jmpbuf[JB_PC] = translate_address((address_t) scheduler);
    scheduler_buf->__jmpbuf[JB_SP] =
        translate_address((address_t)scheduler_stack + 4096 - sizeof(int));
    sigemptyset(&scheduler_buf->__saved_mask);

    running_tcb = new TCB();

    return 0;
}

// put the tcb of passed tid into a clean_tcb list and yield if currently running 
int uthread_terminate(int tid){
  TCB* tcb = find_tcb(tid);
  if (tcb == NULL)
    return -1;

  cleanup_tcb.push_back(tcb);

  if (uthread_self() == tid) {
    uthread_yield();
  }

  return 0;
}

int uthread_suspend(int tid) {
  ControlBlocker c();
  TCB* tcb = find_tcb(tid);

  if (tcb == NULL)
    return -1;

  switch(tcb->get_state()) {
    case READY:  
      queue<TCB*> q; 
      while(!ready_tcb.empty()) {
        TCB* tcb = ready_tcb.front();
        if (tcb->get_tid() != tid) {
          q.push(tcb);
        }
        ready_tcb.pop(); 
      }
      q.swap(ready_tcb); 
    break;
  }

  tcb->set_state(State::SUSPENDED);

  if (tid == uthread_self()) {
    uthread_yield();
  }

  return 0;
}

int uthread_resume(int tid){
  TCB* tcb = find_tcb(tid);

  if (tcb == NULL) {
    return -1;
  }

  if (tcb->get_state() == State::SUSPENDED) {
    tcb->set_state(State::READY);
    ready_tcb.push(tcb);
  }

  return 0;
}

void preempt_thread(int signum) {
  uthread_yield();
}

void run_thread(void) {
  running_tcb->run_stub();
}

void TCB::run_stub(void) {
  retval = (*start_routine_)(arg_);
  uthread_terminate(tid);
}

TCB::TCB() {
  // In the root thread which already has a context
  // and is already running
  tid = next_usable_tid++;
  joining_tid = -1;
  state = State::RUNNING;
  retval = NULL;
  running_tcb = this;
  critical_section = false;
  missed_interrupt = false;
  return;
}


TCB::TCB(void* (*start_routine)(void*), void *arg) {
  // In a new thread which needs a context
  // and isn't running yet
  joining_tid = -1;
  arg_ = arg;
  start_routine_ = start_routine;
  tid = next_usable_tid++;
  state = State::READY;
  retval = NULL;
  sigsetjmp(Buf, 1);
  stack_buf = malloc(STACK_SIZE);

  address_t sp, pc;
  sp = (address_t)stack_buf + STACK_SIZE - sizeof(int); 
  pc = (address_t)run_thread;

  Buf->__jmpbuf[JB_SP] = translate_address(sp);
  Buf->__jmpbuf[JB_PC] = translate_address(pc);

  critical_section = false;
  missed_interrupt = false;
}

int lock_init(lock_t* lock) {
  lock->holding_tid = -1;
  lock->head = NULL;
  lock->tail = NULL;
  return 0;
}

int acquire(lock_t* lock) {
  ControlBlocker c();

  if (lock->holding_tid >= 0) { // Append our TID to the wait list
    lock_node *n = (lock_node*)malloc(sizeof(lock_node));
    n->tid = uthread_self();
    n->next = NULL;

    if (lock->head == NULL) {
      lock->head = n;
      lock->tail = n;
    } else {
      lock->tail->next = n;
      lock->tail = n;
    }

    do { // Wait for our turn
      uthread_suspend(running_tcb->get_tid());
    } while (lock->holding_tid != uthread_self());

  } else { // Lock is ours, take it
    lock->holding_tid = uthread_self();
  }

  return 0;
}

int release(lock_t* lock) {
  ControlBlocker c();
  if (lock->head == NULL) { // No other threads waiting
    lock->holding_tid = -1;
    return 0;
  } else {
    lock->holding_tid = lock->head->tid;
    lock_node *n = lock->head->next;
    if (n == NULL) {
      lock->tail = NULL;
    }
    free(lock->head);
    lock->head = n;
    uthread_resume(lock->holding_tid);
    return 0;
  }
}

ControlBlocker::ControlBlocker() {
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGVTALRM);
  sigprocmask(SIG_BLOCK, &sigset, NULL);
  running_tcb->set_critical(true);
}

ControlBlocker::~ControlBlocker() {
  sigprocmask(SIG_UNBLOCK, &sigset, NULL);
  running_tcb->set_critical(false);
  if (running_tcb->get_interrupt()) {
    uthread_yield();
  }
}
