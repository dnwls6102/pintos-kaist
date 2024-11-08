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

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL); // 세마포어가 NULL이 아닌지 확인.

	sema->value = value; // 세마포어의 초기값을 설정.
	list_init (&sema->waiters); // 대기 리스트 초기화.
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
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable (); // 인터럽트를 비활성화하고 현재 인터럽트 상태 저장
	while (sema->value == 0) { // 세마포어 값이 0인 경우, 즉 자원이 없는 경우
		list_push_back (&sema->waiters, &thread_current ()->elem); // 현재 스레드를 대기 리스트에 추가
		thread_block (); // 현재 스레드를 블록 상태로 전환하여 대기
	}
	sema->value--; // 세마포어 값 감소 (자원 사용)
	intr_set_level (old_level); // 이전 인터럽트 상태로 복원
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
    enum intr_level old_level;

    ASSERT (sema != NULL);

    old_level = intr_disable (); // 인터럽트 비활성화하고 현재 인터럽트 상태 저장
    
    sema->value++; // 세마포어 값 증가 (자원 반환)

    if (!list_empty (&sema->waiters)) {
        // 대기 중인 스레드가 있다면
        // 대기 리스트를 우선순위 순서로 정렬하여 높은 우선순위의 스레드를 깨운다
        list_sort(&sema->waiters, cmp_priority, NULL);
        struct thread *next_thread = list_entry (list_pop_front (&sema->waiters),
                                                 struct thread, elem);
        thread_unblock (next_thread);

        // 방금 깨운 스레드가 현재 실행 중인 스레드보다 높은 우선순위라면, CPU 양보
        if (next_thread->priority > thread_current()->priority) {
            thread_yield();
        }
    }

    intr_set_level (old_level); // 이전 인터럽트 상태로 복원
}

static void
 sema_test_helper (void *sema_);

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

	lock->holder = NULL; // 락 소유자를 NULL로 설정하여 초기 상태에서 락을 가진 스레드가 없도록 함
	sema_init (&lock->semaphore, 1); // 내부 세마포어를 1로 초기화하여 한 번에 하나의 스레드만 접근할 수 있게 설정
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
	ASSERT (lock != NULL);
	ASSERT (!intr_context ()); // 인터럽트 컨텍스트에서 호출되지 않음을 확인
	ASSERT (!lock_held_by_current_thread (lock)); // 현재 스레드가 이미 락을 소유하고 있지 않음을 확인

	sema_down (&lock->semaphore); // 세마포어를 통해 락 획득 시도 (필요시 대기)
	lock->holder = thread_current (); // 현재 스레드를 락의 소유자로 설정
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
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock)); // 현재 스레드가 락의 소유자인지 확인

	lock->holder = NULL; // 락 소유자를 NULL로 설정하여 락을 가진 스레드가 없음을 표시
	sema_up (&lock->semaphore); // 세마포어를 해제하여 대기 중인 스레드를 깨움
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

	list_init (&cond->waiters); // 조건 변수를 기다리는 스레드들의 리스트 초기화
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
    struct semaphore_elem waiter;

    ASSERT (cond != NULL);               // 조건 변수가 NULL이 아닌지 확인
    ASSERT (lock != NULL);               // 락이 NULL이 아닌지 확인
    ASSERT (!intr_context ());           // 인터럽트 상태가 아님을 확인
    ASSERT (lock_held_by_current_thread (lock));  // 락이 현재 스레드에 의해 소유 중인지 확인

    // 세마포어 초기화 (0으로 설정하여 대기 상태를 나타냄)
    sema_init (&waiter.semaphore, 0);

    // 조건 변수를 기다리는 스레드 리스트에 현재 스레드를 추가
    list_push_back (&cond->waiters, &waiter.elem);

    // 락을 해제하여 다른 스레드가 자원에 접근할 수 있도록 함
    lock_release (lock);

    // 세마포어 대기 상태로 전환되어 다른 스레드로부터 신호를 기다림
    sema_down (&waiter.semaphore);

    // 신호를 받은 후 락을 다시 획득하여 자원을 안전하게 사용 가능
    lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */

void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
    ASSERT (cond != NULL);               // 조건 변수가 NULL이 아닌지 확인
    ASSERT (lock != NULL);               // 락이 NULL이 아닌지 확인
    ASSERT (!intr_context ());           // 인터럽트 상태가 아님을 확인
    ASSERT (lock_held_by_current_thread (lock));  // 락이 현재 스레드에 의해 소유 중인지 확인

    // 조건을 기다리는 스레드가 있다면, 가장 먼저 대기한 스레드를 깨움
    if (!list_empty (&cond->waiters)) {

		// 조건 변수의 대기 리스트를 우선순위 순서로 정렬하여 높은 우선순위의 스레드를 깨운다
        list_sort(&cond->waiters, cmp_priority, NULL);

        // 대기 목록에서 가장 앞에 있는 스레드를 제거하여 신호를 보냄
        sema_up (&list_entry (list_pop_front (&cond->waiters), struct semaphore_elem, elem)->semaphore);
    }
}


/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
    ASSERT (cond != NULL);               // 조건 변수가 NULL이 아닌지 확인
    ASSERT (lock != NULL);               // 락이 NULL이 아닌지 확인

    // 대기 목록에 스레드가 존재하는 동안 반복하여 신호를 보냄
    while (!list_empty (&cond->waiters))
        cond_signal (cond, lock);       // 대기 중인 스레드를 깨우기 위해 cond_signal 호출
}
