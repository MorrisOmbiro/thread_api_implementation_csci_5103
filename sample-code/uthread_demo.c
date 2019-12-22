#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#define SECOND 1000000
#define STACK_SIZE 4096

int TAS(volatile int *addr, int newval){
    int result = newval;
    asm volatile("lock; xchg %0, %1"
                 : "+m" (*addr), "=r" (result)
                 : "1" (newval)
                 : "cc");
    return result;
                
}


// stacks for threads
char *stack1;
char *stack2;

sigjmp_buf jbuf[2];

void switchThreads();

void f()
{
    int i=0;
    while(1) {
        ++i;
        printf("in f (%d)\n",i);
        if (i % 3 == 0) {
            printf("f: switching\n");
            switchThreads();
        }
        sleep(1);
    }
}

void g()
{
    int i=0;
    while(1){
        ++i;
        printf("in g (%d)\n",i);
        if (i % 5 == 0) {
            printf("g: switching\n");
            switchThreads();
        }
        sleep(1);
    }
}


#ifdef __x86_64__


typedef unsigned long address_t;
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
	address_t ret;
	asm volatile("xor    %%fs:0x30,%0\n"
			"rol    $0x11,%0\n"
			: "=g" (ret)
			  : "0" (addr));
	return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
	address_t ret;
	asm volatile("xor    %%gs:0x18,%0\n"
			"rol    $0x9,%0\n"
			: "=g" (ret)
			  : "0" (addr));
	return ret;
}

#endif

void setup()
{
    address_t sp, pc;
    
    // stack pointer is calculated this way because on Linux, the stack pointer
    // grows from high address to low address.
    sp = (address_t)stack1 + STACK_SIZE - sizeof(int); 
    pc = (address_t)f;
    
    sigsetjmp(jbuf[0],1);

    // the index is recorded in glibc soucre code, if you are interested, check
    // glibc source code /x86_64/jmpbuf-offsets.h
    // the reason why we need translate_address() function is because glibc
    // mangles(i.e. encrypts) pointer in jmpbuf to ensure security
    (jbuf[0]->__jmpbuf)[JB_SP] = translate_address(sp);
    (jbuf[0]->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&jbuf[0]->__saved_mask);     


    sp = (address_t)stack2 + STACK_SIZE - sizeof(int);
    pc = (address_t)g;
    
    sigsetjmp(jbuf[1],1);
    (jbuf[1]->__jmpbuf)[JB_SP] = translate_address(sp);
    (jbuf[1]->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&jbuf[1]->__saved_mask);         
}


void switchThreads()
{
    static int currentThread = 0;
    
    int ret_val = sigsetjmp(jbuf[currentThread],1);
    printf("SWITCH: ret_val=%d\n", ret_val); 
    if (ret_val == 1) {
        return;
    }
    
    currentThread = 1 - currentThread;
    siglongjmp(jbuf[currentThread],1);
}


int main()
{
    stack1 = (char *)malloc(STACK_SIZE);
    stack2 = (char *)malloc(STACK_SIZE);

    setup();		
    siglongjmp(jbuf[0],1);
    return 0;
}






