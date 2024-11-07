/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#define TEST

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
void
sema_down (struct semaphore *sema) {
	#ifndef TEST
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable ();
	while (sema->value == 0) {
		list_push_back (&sema->waiters, &thread_current ()->elem);
		thread_block ();
	}
	sema->value--;
	intr_set_level (old_level);
	#endif
	#ifdef TEST
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable ();
	//만약 세마포어 값이 0이라면
	while (sema->value == 0) {
		//현재 락(세마포어)의 waiters에 현재 스레드 삽입
		list_insert_ordered (&sema->waiters, &thread_current ()->elem, priority_more, NULL);
		//현재 스레드의 상태를 BLOCKED로 설정
		thread_block ();
	}
	sema->value--;
	intr_set_level (old_level);
	#endif
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) {
	#ifndef TEST
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (!list_empty (&sema->waiters))
		thread_unblock (list_entry (list_pop_front (&sema->waiters),
					struct thread, elem));
	sema->value++;
	intr_set_level (old_level);
	#endif
	#ifdef TEST
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (!list_empty (&sema->waiters))
	{
		//waiters에서 원소를 빼주기 전에 sort : 우선순위 전이가 일어났을수도
		list_sort(&sema->waiters, priority_more, NULL);
		thread_unblock (list_entry (list_pop_front (&sema->waiters),
					struct thread, elem));
	}
	sema->value++;
	intr_set_level (old_level);
	#endif
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock) {
	#ifdef TEST
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));

	sema_down (&lock->semaphore);
	lock->holder = thread_current ();
	#endif
	#ifndef TEST
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));

	struct thread * current_t = thread_current();
	//만약 세마포어 값이 0이하(사실 여기서는 단순히 락을 위한 이진 세마포어로 사용되기 때문에 0 아래로 내려갈 일은 없긴 함)
	if (lock -> semaphore.value <= 0)
	{
		
		//현재 스레드의 wait_on_lock을 현재 요구한 lock으로 설정
		current_t -> wait_on_lock = lock;
		/*
		현재 스레드의 상태를 BLOCK으로 설정하고
		락(세마포어)의 waiters 리스트에 현재 스레드를 삽입하는 코드는
		sema_down에 구현되어 있음
		*/
		//현재 스레드의 original_priority에 기존 우선순위 저장
		//lock_acquire를 실행하여 두 개 이상의 락을 보유하는 상황이면
		//원본 original priority가 지워지진 않을지?
		//이를 방지하기 위해 original_priority의 기본값을 -1로 설정시키고
		//-1인 경우에만 original_priority를 저장시키기
		if (current_t -> original_priority == -1)
			current_t -> original_priority = current_t -> priority;
		//현재 lock을 보유하고 있는 스레드보다 우선순위가 높다면 : 일단 기부
		if (lock->holder->priority < current_t -> priority)
		{
			lock->holder->priority = current_t -> priority;
			//lock 소유자 스레드의 donations 리스트에 현재 스레드 추가
			list_insert_ordered(&(lock->holder->donations), &(current_t -> elem), priority_more, NULL);
		}
		/*
		sema->waiters가 우선순위에 대한 내림차순으로 정렬이 되어있는데
		굳이 우선순위 전이를 시키고 donation 리스트에 넣는 이유
		현재 락을 소유한 스레드가 실행중인 스레드라면 사실 전혀 상관이 없다
		그렇다면 waiters의 head에 위치한 스레드에게 락을 넘겨주고
		해당 스레드를 ready_list로 옮겨주면(lock_release)
		문제 없이 우선순위대로 실행이 된다

		하지만 만약 락을 소유중인 스레드가 실행중인 스레드가 아니고
		ready_list에 락을 소유중인 스레드보다는 우선순위가 높지만
		waiters의 head에 위치한 스레드의 우선순위보다는 낮은 스레드들이 앞서 있다면
		그 스레드들보다 waiters의 head에 위치한 스레드를 먼저 실행시켜야 한다

		락을 넘겨줬다는 것은 곧 임계 영역에 대한 작업이 모두 완료되었다는 것이기에
		만약 donations에 해당 영역에 대하여 락을 요청한 스레드들이 남아있다면
		모두 삭제해줘야 한다

		락을 요청했지만, 해당 영역에 대한 락을 요청한 것은 아니라면
		우선순위 기부를 다시 받고 넘어가야 한다
		*/

	}
	sema_down (&lock->semaphore);
	lock->holder = current_t;
	#endif

}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) {
	#ifdef TEST
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	lock->holder = NULL;
	sema_up (&lock->semaphore);
	#endif
	#ifndef TEST
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	struct thread* old_holder = lock -> holder;
	struct thread* temp_thread;
	struct list_elem * temp;
	lock->holder = NULL;
	sema_up (&lock->semaphore);

	old_holder -> priority = old_holder -> original_priority;
	old_holder -> original_priority = -1;

	if(!list_empty(&old_holder -> donations))
	{
		temp = list_front(&(old_holder -> donations));

		//기부받았던 스레드의 삭제
		while (1)
		{
			temp_thread = list_entry(temp, struct thread, elem);
			if (lock == temp_thread -> wait_on_lock && old_holder -> priority == temp_thread -> priority)
			{
				list_remove(temp);
				break;
			}
			else
				temp = temp -> next;
		}	
		//현재 스레드에 우선순위를 기부받을(경쟁 조건에 의해 )
		struct thread *donation_top = list_entry(list_front(&old_holder->donations), struct thread, elem);
		//만약 donation_top의 우선순위가 새로 부여받을 우선순위보다 높다면
		if (donation_top -> priority > old_holder -> priority)
			old_holder -> priority = donation_top -> priority; //donation_top의 우선순위로 현재 스레드의 우선순위를 변경
	}
	#endif
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	struct semaphore semaphore;         /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) {
	#ifndef TEST
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
	list_push_back (&cond->waiters, &waiter.elem);
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
	#endif
	#ifdef TEST
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
	//condition variable의 wait_list의 맨 앞에 우선순위가 가장 높은 스레드가 들어가게끔
	list_insert_ordered (&cond->waiters, &waiter.elem, priority_more, NULL);
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
	#endif
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	#ifndef TEST
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters))
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
	#endif
	#ifdef TEST
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters))
	{
		//cond_signal에 sort를 추가 : 우선순위 전이가 일어났을수도 있으니까
		list_sort(&cond->waiters, priority_more, NULL);
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
	}
	#endif
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}