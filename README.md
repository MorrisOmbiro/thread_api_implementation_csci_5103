CSCI 5103
Project 1
Members
Webster Wing wing038
Morris Ombiro ombir002

- What assumptions did we make 
1. No memory limitation to the number of threads we received. 
2. One thread on the processor at a given time 
3. We assume that we never return from uthread_yield due to the context jump from the siglongjmp function 
4. That we may not terminate a thread in a critical state otherwise bad things will happen. Instead we let it finish before we queue it in a cleanup_tcb list (no order needed).

- Descripe the API, including input, output, return value
1. The input to our API is simply the number of threads expected to execute. 
2. The output is a string indicating the thread that's currently running. Depending on the test, the output willl maybe mention that we're: inside thread n. If it's a lock test then we may print: thread n has acquired the lock, and then thread n has released the lock. 
3. Return Values: 
Uthread_create -> returns the tid of a thread 
uthread_yield -> Returns a 0 if it fails otherwise we jump context using siglongjmp 
4. uthread_self -> Returns the tcb of the currently running thread 
5. uthread_init -> Returns 0 on success 
6. uthread_terminate -> Returns 0 on success 
7. uthread_suspend -> Returns 0 on success 
8. uthread_resume -> Returns 0 on success and -1 on fail 
9. lock_init -> returns 0 on success 
10. acquire -> returns 0 on success 
11. release -> returns 0 on success 

- Did we add any customized APIs? What functionalities do they provide 
We did not provide any customized API 

- How did your library pass input/output parameters to a thread entry function? 
We simply used a void* to pass values to a thread entry function and the n returned a void* as well. Easier conversion to any other type.  

- How do different lengths of time slice affect performance? 
I tested with a time slice of 1us 1000us and 1000000us and there was not too much of a noticeable difference as I would have expected. 

- What are the critical sections your code has protected against interrupts and why? 
1. release(lock_t* lock); if we get interrupted we would have the next waiting thread waiting indefinitely for a lock to acquire.
2. acquire(lock_t* lock); We don't want a condition where two locks are trying to acquire the lock at the same time.
3. uthread_suspend(int tid); We are storing the context of a thread in this case which will later be resumed using uthread_resume(int tid) function. If we do not make this critical then we can't be sure that the context will be the same on thread_resume(int tid). 
4. We also have a critical section when the currently running_tcb is saving its context when the scheduler gets called; again, to prevent corruption of a thread's contexts. 
5. Ensuring we can't terminate a thread if the thread is in a critical section. Wait until its done before terminating it. If this happens and if this thread had a lock, the lock would never be released. 

- If you have implemented more advanced scheduling algorithm(s), describe them
No just a FIFO through a queue. We have a separate data structure for terminated threads; a list. It doesn't really matter the order in which they are terminated as long as they are in this list, we'll clean them out next time the scheduler loops through the clenup list. 

Does your library allow each thread to have its own signal mask, or do all threads share the same signal mask? How did you guarantee this? 
Yes we allow each thread to have its own signal mask. During uthread_init(int timeslice). We save the signal mask of each thread and call the scheduler which implments a signal mask for each thread. We can guarantee when using the sigsetjmp which saves the signal mask while storing the calling environment. We call this function in the implementation of TCB for each thread.


