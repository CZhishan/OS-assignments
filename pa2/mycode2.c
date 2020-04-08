/* mycode2.c: your portion of the kernel
 *
 *   	Below are functions that are called by other parts of the kernel. 
 * 	Your ability to modify the kernel is via these functions.  You may
 *  	modify the bodies of these functions (and add code outside of them)
 *  	in any way you wish (however, you cannot change their interfaces).  
 */

#include "aux.h"
#include "sys.h"
#include "mycode2.h"

#define TIMERINTERVAL 1	// in ticks (tick = 10 msec)
#define div 10000
#define UINT_MAX 4294967295

/* 	A sample process table. You may change this any way you wish. 
 */

static struct {
	int valid;		// is this entry valid: 1 = yes, 0 = no
	int pid;		// process ID (as provided by kernel)
} proctab[MAXPROCS];

static struct {
	int valid;
	int pid;
} procque[MAXPROCS+1];

static int head, tail, top;

static struct {
	int valid;
	int pid;
	int req;
	int stride;
	unsigned long long passval;
} proctab_prop[MAXPROCS];

void ComputeStride();
static int curReq;
static int curProc;

/* 	InitSched() is called when the kernel starts up. First, set the
 * 	scheduling policy (see sys.h). Make sure you follow the rules
 *   	below on where and how to set it.  Next, initialize all your data
 * 	structures (such as the process table).  Finally, set the timer
 *  	to interrupt after a specified number of ticks. 
 */

void InitSched()
{
	int i;

	/* First, set the scheduling policy. You should only set it
	 * from within this conditional statement. While you are working
	 * on this assignment, GetSchedPolicy() will return NOSCHEDPOLICY. 
	 * Thus, the condition will be true and you may set the scheduling
	 * policy to whatever you choose (i.e., you may replace ARBITRARY).  
	 * After the assignment is over, during the testing phase, we will
	 * have GetSchedPolicy() return the policy we wish to test (and
	 * the policy WILL NOT CHANGE during the entirety of a test).  Thus
	 * the condition will be false and SetSchedPolicy(p) will not be
	 * called, thus leaving the policy to whatever we chose to test
	 * (and so it is important that you NOT put any critical code in
	 * the body of the conditional statement, as it will not execute when
	 * we test your program). 
	 */
	if (GetSchedPolicy() == NOSCHEDPOLICY) {	// leave as is
							// no other code here
		SetSchedPolicy(PROPORTIONAL);		// set policy here
							// no other code here
	}
		
	/* Initialize all your data structures here */
	for (i = 0; i < MAXPROCS; i++) {
		proctab[i].valid = 0;
		procque[i].valid = 0;
		proctab_prop[i].valid = 0;
		proctab_prop[i].req = 0;
		proctab_prop[i].passval = 0;
		
	}
	
	head = 0;
	tail = 0;
	top = -1;
	curReq = 0;
	curProc = 0;

	/* Set the timer last */
	SetTimer(TIMERINTERVAL);
}

/* My helper function to compute stride when processes create or exit or request*/

void ComputeStride() {
	int i, left, count1, count2;
	left = 100 - curReq;
	
	for (i = 0, count1 = 0, count2 = 0; i < MAXPROCS; i++) {
		if (proctab_prop[i].valid) {
			proctab_prop[i].passval = 0;
			if (proctab_prop[i].req) { 
				count1 += 1;
			}
			else {
				count2 += 1;
			}
		}
	}
	
	for (i = 0; i < MAXPROCS; i++) {
		if (proctab_prop[i].valid) {
			if (proctab_prop[i].req) {
				if (count2 == 0 && left) {
					proctab_prop[i].stride = div / (proctab_prop[i].req + left / count1);
				}
				else {
					proctab_prop[i].stride = div / proctab_prop[i].req;
				}
				
			}
			else if (count2) {
				if (left) {
					proctab_prop[i].stride = div * count2 / left;
				}
				else {
					proctab_prop[i].stride = div * MAXPROCS;
				}
			}
			//DPrintf("stride:%d\n",proctab_prop[i].stride);
		}
	}
}


/*  	StartingProc(p) is called by the kernel when the process
 * 	identified by PID p is starting. This allows you to record the
 * 	arrival of a new process in the process table, and allocate any
 * 	resources (if necessary). Returns 1 if successful, 0 otherwise. 
 */

int StartingProc(int p) 		
	// p: process that is starting
{
	int i;
	
	switch(GetSchedPolicy()) {
		case ARBITRARY:
			for (i = 0; i < MAXPROCS; i++) {
				if (! proctab[i].valid) {
					proctab[i].valid = 1;
					proctab[i].pid = p;			
					return (1);
				}
			}
			break;
			
		case FIFO :
		case ROUNDROBIN:
			if (((tail + 1) % (MAXPROCS + 1) != head) && (! procque[tail].valid)) {
				procque[tail].valid = 1;
				procque[tail].pid = p;
				tail = (tail + 1) % (MAXPROCS + 1);
				return(1);
			}
			break;
			
		case LIFO:
			if ((top != (MAXPROCS - 1)) && (! proctab[top + 1].valid)) {
				top += 1;
				proctab[top].valid = 1;
				proctab[top].pid = p;
				
				DoSched();
				return(1);
			}
			break;
			
		case PROPORTIONAL:
			for (i = 0; i < MAXPROCS; i++) {
				if (! proctab_prop[i].valid) {
					proctab_prop[i].valid = 1;
					proctab_prop[i].pid = p;

					ComputeStride();
					return (1);
				}
			}
			break;
	}
	
	DPrintf("Error in StartingProc: no free table entries\n");
	return(0);
}
			

/*   	EndingProc(p) is called by the kernel when the process
 * 	identified by PID p is ending.  This allows you to update the
 *  	process table accordingly, and deallocate any resources (if
 *  	necessary).  Returns 1 if successful, 0 otherwise. 
 */


int EndingProc(int p)
	// p: process that is ending
{
	int i;

	switch(GetSchedPolicy()) {
		case ARBITRARY:
			for (i = 0; i < MAXPROCS; i++) {
				if (proctab[i].valid && proctab[i].pid == p) {
					proctab[i].valid = 0;
					return(1);
				}
			}
			break;
			
		case FIFO:
			if (tail != head && procque[head].valid) {
				procque[head].valid = 0;
				head = (head + 1) % (MAXPROCS + 1);

				return(1);
			}
			break;
			
		case LIFO:
			if (top != -1 && proctab[top].valid) {
				proctab[top--].valid = 0;

				return(1);
			}
			break;
		
		case ROUNDROBIN:
			for (i = 0; i <= MAXPROCS; i++) {
				if (procque[i].valid && procque[i].pid == p) {
					procque[i].valid = 0;

					return(1);
				}
			}
			break;
			
		case PROPORTIONAL:
			for (i = 0; i < MAXPROCS; i++) {
				if (proctab_prop[i].valid && proctab_prop[i].pid == p) {
					proctab_prop[i].valid = 0;
					curReq -= proctab_prop[i].req;

					ComputeStride();
					return(1);
				}
			}
			break;
	}

	DPrintf("Error in EndingProc: can't find process %d\n", p);
	return(0);
}


/* 	SchedProc() is called by kernel when it needs a decision for
 * 	which process to run next. It will call the kernel function
 * 	GetSchedPolicy() which will return the current scheduling policy
 *   	which was previously set via SetSchedPolicy(policy). SchedProc()
 * 	should return a process PID, or 0 if there are no processes to run. 
 */

int SchedProc()
{
	int i, index;
	unsigned long long min = UINT_MAX;

	switch(GetSchedPolicy()) {

	case ARBITRARY:

		for (i = 0; i < MAXPROCS; i++) {
			if (proctab[i].valid) {
				return(proctab[i].pid);
			}
		}
		break;

	case FIFO:

		/* your code here */
		if (procque[head].valid) {
			return(procque[head].pid);
		}

		break;

	case LIFO:

		/* your code here */
		if (proctab[top].valid) {
			return(proctab[top].pid);
		}

		break;

	case ROUNDROBIN:

		/* your code here */
		if (procque[head].valid) {                  // move head to tail
			i = procque[head].pid;
			procque[head].valid = 0;
			procque[tail].valid = 1;
			procque[tail].pid = i;
			tail = (tail + 1) % (MAXPROCS + 1);
		}
		
		head = (head + 1) % (MAXPROCS + 1);
		if (procque[head].valid) {
			// DPrintf("pid%d\n",procque[head].pid);
			return(procque[head].pid);
		}

		break;

	case PROPORTIONAL:

		/* your code here */
		index = (curProc + 1) % MAXPROCS;
		for (i = 0; i < MAXPROCS; i++) {
			if (proctab_prop[index].valid) {
				if (proctab_prop[index].passval < min) {
					min = proctab_prop[index].passval;
					curProc = index;
				}
			}
			index = (index + 1) % MAXPROCS;
		}
		
		if (proctab_prop[curProc].valid) {
			if (UINT_MAX - proctab_prop[curProc].passval <= proctab_prop[curProc].stride) {
				for (i = 0; i < MAXPROCS; i++) {
					if (proctab_prop[i].valid) {
						proctab_prop[i].passval -= min;
					}
				}
			}
			
			proctab_prop[curProc].passval += proctab_prop[curProc].stride;
			return(proctab_prop[curProc].pid);
		}
		break;

	}
	
	return(0);
}


/*  	HandleTimerIntr() is called by the kernel whenever a timer
 *  	interrupt occurs.  Timer interrupts should occur on a fixed
 * 	periodic basis.
 */

void HandleTimerIntr()
{
	SetTimer(TIMERINTERVAL);

	switch(GetSchedPolicy()) {	// is policy preemptive?

	case ROUNDROBIN:		// ROUNDROBIN is preemptive
	case PROPORTIONAL:		// PROPORTIONAL is preemptive

		DoSched();		// make scheduling decision
		break;

	default:			// if non-preemptive, do nothing
		break;
	}
}

/* 	MyRequestCPUrate(p, n) is called by the kernel whenever a process
 * 	identified by PID p calls RequestCPUrate(n).  This is a request for
 *   	n% of CPU time, i.e., requesting a CPU whose speed is effectively
 * 	n% of the actual CPU speed. Roughly n out of every 100 quantums
 *  	should be allocated to the calling process. n must be at least
 *  	0 and must be less than or equal to 100. MyRequestCPUrate(p, n)
 * 	should return 0 if successful, i.e., if such a request can be
 * 	satisfied, otherwise it should return -1, i.e., error (including
 * 	if n < 0 or n > 100). If MyRequestCPUrate(p, n) fails, it should
 *   	have no effect on the scheduling of this or any other process,
 * 	i.e., AS IF IT WERE NEVER CALLED.
 */

int MyRequestCPUrate(int p, int n)
	// p: process whose rate to change
	// n: percent of CPU time
{
	/* your code here */
	if (n < 0 || n > 100) {
		return(-1);
	}
	
	for (int i = 0; i < MAXPROCS; i++) {
		if (proctab_prop[i].pid == p) {
			if (curReq - proctab_prop[i].req + n > 100) {
				return(-1);
			}
			else {
				curReq = curReq - proctab_prop[i].req + n;
				proctab_prop[i].req = n;
			}
			break;
		}
	}
	ComputeStride();
	
	return(0);
}
