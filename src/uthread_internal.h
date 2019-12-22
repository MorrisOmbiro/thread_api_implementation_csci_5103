//
// Created by morriskalb on 10/11/2019.
//

#ifndef CSCI5103_P1_UTHREAD_INTERNAL_H
#define CSCI5103_P1_UTHREAD_INTERNAL_H
#include <setjmp.h>
#include <signal.h>
#include <cstdlib>



// able to manipulate the signal mask of the context
typedef unsigned long address_t;
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr) {
	address_t ret;
	asm volatile("xor    %%fs:0x30,%0\n"
			"rol    $0x11,%0\n"
			: "=g" (ret)
			  : "0" (addr));
	return ret;
}

enum State{READY, RUNNING, SUSPENDED, TERMINATED};
// Thread entry function: 
// entry function input parameters 
// Every function's return value (pointer to output data)
class TCB {
private:
  int tid;
  sigjmp_buf Buf; 
  State state; 
  int joining_tid; // keeps track of which thread is waiting on this one
  void *arg_, *retval, *stack_buf;
  void* (*start_routine_)(void*);
  bool critical_section; // Ensures that this thread isn't interrupted
  // During operations which must be atomic
  bool missed_interrupt;
public:
    TCB();
    TCB(void* (*start_routine)(void*), void *arg);

  void set_joining_with(int t) {
    joining_tid = t;
  }
    
    // this is a change 
    int get_tid() {
        return tid;
    }

    void set_state(enum State s) {
        state = s;
    }

    enum State get_state() {
        return state; 
    }

  void terminate() {
    state = State::TERMINATED;

    if (state != TERMINATED) {
      free(stack_buf);

      if (joining_tid >= 0) {
        uthread_resume(joining_tid);
     }
    }
    
  }

  sigjmp_buf* get_buf(void) {
    return &Buf;
  }

  void populate_ret_ptr(void** ret) {
    if (ret != NULL)
      *ret = retval;
  }
      
  bool get_critical() {
    return critical_section;
  }

  void set_critical(bool c) {
    critical_section = c;
  }

  void set_interrupt() {
    missed_interrupt = true;
  }

  bool get_interrupt() {
    return missed_interrupt;
  }

  void interrupted() {
    missed_interrupt = false;
  }
    
  void run_stub(void);
};


#endif //CSCI5103_P1_UTHREAD_INTERNAL_H
