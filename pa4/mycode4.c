/* mycode4.c: UMIX thread package
 *
 *   	Below are functions that comprise the UMIX user-level thread package. 
 * 	These functions are called by user programs that use threads.  You may
 *  	modify the bodies of these functions (and add code outside of them)
 *  	in any way you wish (however, you cannot change their interfaces).  
 */

#include <setjmp.h>
#include <string.h>

#include "aux.h"
#include "umix.h"
#include "mycode4.h"

static int MyInitThreadsCalled = 0;	// 1 if MyInitThreads called, else 0

static struct thread {			// thread table
	int valid;			// 1 if entry is valid, else 0
	jmp_buf env;			// current context
	jmp_buf orig_env;
	void (*func)();
	int param;
} thread[MAXTHREADS];

#define STACKSIZE	65536		// maximum size of thread stack 65536

static int cur_thread, prev_thread;
static int thread_q[MAXTHREADS];
static int tail, last_create;
static int exitFlag, schedFlag;

void printq() 
{
	DPrintf("current q:");
	for (int i = 0; i < MAXTHREADS; i++) {
		DPrintf("%d", thread_q[i]);
	}
	DPrintf("\n");
}

/* 	MyInitThreads() initializes the thread package. Must be the first
 * 	function called by any user program that uses the thread package. 
 */

void MyInitThreads()
{
	int i;

	if (MyInitThreadsCalled) {		// run only once
		Printf("MyInitThreads: should be called only once\n");
		Exit();
	}
	
	MyInitThreadsCalled = 1;
	tail = 0;
	exitFlag = 0;
	schedFlag = 0;
	
	for (i = 0; i < MAXTHREADS; i++) {
		thread_q[i] = -1;
	}

	for (i = 0; i < MAXTHREADS; i++) {	// initialize thread table
		thread[i].valid = 0;
		char stack[STACKSIZE * i];
		//void (*f)();	
		//int p;		

		
		if (((int) &stack[STACKSIZE*i-1]) - ((int) &stack[0]) + 1 != STACKSIZE*i) {
			Printf("Stack space reservation failed\n");
			Exit();
		}  

		
		if (setjmp(thread[i].orig_env) != 0) {
						
			//f = thread[cur_thread].func;
			//p = thread[cur_thread].param;
			//(*f)(p);			// execute func(param)
			(*thread[cur_thread].func)(thread[cur_thread].param);
			
			MyExitThread();	
		}
		
	}
	
	//memcpy(thread[0].env, thread[0].orig_env, sizeof(jmp_buf));
	thread[0].valid = 1;			// initialize thread 0
	cur_thread = 0;
	last_create = 0;
}

/* 	MyCreateThread(f, p) creates a new thread to execute f(p),
 *   	where f is a function with no return value and p is an
 * 	integer parameter. The new thread does not begin executing
 *  	until another thread yields to it. 
 */

int MyCreateThread(void (*f)(), int p)
	// f: function to be executed
	// p: integer parameter
{
	if (! MyInitThreadsCalled) {
		Printf("MyCreateThread: Must call MyInitThreads first\n");
		Exit();
	}

	if (tail == (MAXTHREADS-1)) {
		return(-1);
	}
	
	int tid = last_create;
	for (int j = 0; j < MAXTHREADS; j++) {
		tid = (tid + 1) % MAXTHREADS;
		if (! thread[tid].valid) {
			
			memcpy(thread[tid].env, thread[tid].orig_env, sizeof(jmp_buf));
			thread[tid].valid = 1;
			thread[tid].func = f;	
			thread[tid].param = p;
			
			thread_q[tail] = tid;
			tail++;
			break;
		}
	}
	//printq();
	last_create = tid;
	
	return(tid);
}

/*  	MyYieldThread(t) causes the running thread, call it T, to yield to
 * 	thread t.  Returns the ID of the thread that yielded to the calling
 * 	thread T, or -1 if t is an invalid ID.  Example: given two threads
 * 	with IDs 1 and 2, if thread 1 calls MyYieldThread(2), then thread 2
 *   	will resume, and if thread 2 then calls MyYieldThread(1), thread 1
 * 	will resume by returning from its call to MyYieldThread(2), which
 *  	will return the value 2.
 */

int MyYieldThread(int t)
	// t: thread being yielded to
{
	if (! MyInitThreadsCalled) {
		Printf("MyYieldThread: Must call MyInitThreads first\n");
		Exit();
	}

	if (t < 0 || t >= MAXTHREADS) {
		Printf("MyYieldThread: %d is not a valid thread ID\n", t);
		return(-1);
	}
	if (! thread[t].valid) {
		Printf("MyYieldThread: Thread %d does not exist\n", t);
		return(-1);
	}
	
	if (t == cur_thread) {
		return(t);
	}
	
	int i, j;
	
	// remove t, move others forward, put current at end
	for (i = 0; i < MAXTHREADS; i++) {
		if (thread_q[i] == t) 
			break;
	}

	for (j = i; j < tail - 1; j++) {
		thread_q[j] = thread_q[j+1];
	}
	
	if (exitFlag) {
		thread_q[j] = -1;
		tail--;
		prev_thread = -1;
		exitFlag = 0;
	} else {
		thread_q[j] = cur_thread;
		prev_thread = cur_thread;
	}
	
	if (schedFlag) {
		prev_thread = -1;
		schedFlag = 0;
	}

	
	//DPrintf("current thread:%d, yeilding to %d\n", cur_thread, t);
	//printq();
	
    if (setjmp(thread[cur_thread].env) == 0) {
		cur_thread = t;
		longjmp(thread[t].env, 1);
    }
	

	return(prev_thread);

}

/*  	MyGetThread() returns ID of currently running thread. 
 */

int MyGetThread()
{
	if (! MyInitThreadsCalled) {
		Printf("MyGetThread: Must call MyInitThreads first\n");
		Exit();
	}
	
	return(cur_thread); 
}

/* 	MySchedThread() causes the running thread to simply give up the
 * 	CPU and allow another thread to be scheduled. Selecting which
 * 	thread to run is determined here. Note that the same thread may
 *   	be chosen (as will be the case if there are no other threads). 
 */

void MySchedThread()
{
	if (! MyInitThreadsCalled) {
		Printf("MySchedThread: Must call MyInitThreads first\n");
		Exit();
	}
	
	if (tail == 0) {
		return;
	}
	else {
		schedFlag = 1;
		MyYieldThread(thread_q[0]);
	}
	
}

/* 	MyExitThread() causes the currently running thread to exit.  
 */

void MyExitThread()
{
	if (! MyInitThreadsCalled) {
		Printf("MyExitThread: Must call MyInitThreads first\n"); 
		Exit();
	}
	
	if (tail == 0) {
		
		thread[cur_thread].valid = 0;
		Exit();
	}
	else {
		//printq();
		thread[cur_thread].valid = 0;
		exitFlag = 1;
		MySchedThread();
	}
	
}
