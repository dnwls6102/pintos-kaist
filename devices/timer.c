#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/synch.h"
#include "threads/thread.h"

/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

// #define DEBUG_MLFQS

/* Number of timer ticks since OS booted. */
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);

/* Sets up the 8254 Programmable Interval Timer (PIT) to
   interrupt PIT_FREQ times per second, and registers the
   corresponding interrupt. */
void
timer_init (void) {
	/* 8254 input frequency divided by TIMER_FREQ, rounded to
	   nearest. */
	uint16_t count = (1193180 + TIMER_FREQ / 2) / TIMER_FREQ;

	outb (0x43, 0x34);    /* CW: counter 0, LSB then MSB, mode 2, binary. */
	outb (0x40, count & 0xff);
	outb (0x40, count >> 8);

	intr_register_ext (0x20, timer_interrupt, "8254 Timer");
}

/* Calibrates loops_per_tick, used to implement brief delays. */
void
timer_calibrate (void) {
	unsigned high_bit, test_bit;

	ASSERT (intr_get_level () == INTR_ON);
	printf ("Calibrating timer...  ");

	/* Approximate loops_per_tick as the largest power-of-two
	   still less than one timer tick. */
	loops_per_tick = 1u << 10;
	while (!too_many_loops (loops_per_tick << 1)) {
		loops_per_tick <<= 1;
		ASSERT (loops_per_tick != 0);
	}

	/* Refine the next 8 bits of loops_per_tick. */
	high_bit = loops_per_tick;
	for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
		if (!too_many_loops (high_bit | test_bit))
			loops_per_tick |= test_bit;

	printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}


/* Returns the number of timer ticks since the OS booted. */
int64_t
timer_ticks (void) {
	enum intr_level old_level = intr_disable ();
	int64_t t = ticks;
	intr_set_level (old_level);
	barrier ();
	return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
int64_t
timer_elapsed (int64_t then) {
	return timer_ticks () - then;
}

/* Suspends execution for approximately TICKS timer ticks. */
void 
timer_sleep(int64_t ticks) {
	#ifndef DEBUG_MLFQS
    if (ticks <= 0) {
        return;
    }

    int64_t start = timer_ticks();
    enum intr_level old_level = intr_disable();  // 인터럽트 비활성화

    /* 현재 tick에 대기할 tick을 더하여 wake-up tick 계산 */
    int64_t wakeup_tick = start + ticks;

    /* 스레드를 슬립 상태로 전환 */
    thread_sleep(wakeup_tick);

    intr_set_level(old_level);  // 인터럽트 복원
	#endif
	#ifdef DEBUG_MLFQS
	int64_t start = timer_ticks ();

	ASSERT (intr_get_level () == INTR_ON);
	/** project1-Alarm Clock 
	while (timer_elapsed (start) < ticks)
		thread_yield (); */
	thread_sleep (start + ticks);
	#endif
}


/* Suspends execution for approximately MS milliseconds. */
void
timer_msleep (int64_t ms) {
	real_time_sleep (ms, 1000);
}

/* Suspends execution for approximately US microseconds. */
void
timer_usleep (int64_t us) {
	real_time_sleep (us, 1000 * 1000);
}

/* Suspends execution for approximately NS nanoseconds. */
void
timer_nsleep (int64_t ns) {
	real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void
timer_print_stats (void) {
	printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* Timer interrupt handler. */
static void 
timer_interrupt(struct intr_frame *args UNUSED) {
    ticks++;
	thread_tick(); //4틱이 지나면 다른 프로세스로 전환

	//만약 mlfqs 방식이라면
	if(thread_mlfqs)
	{
		//만약 현재 스레드가 idle 스레드가 아니면
		//현재 스레드의 recent_cpu 수치를 1 올려주기
		if (!is_idle())
			thread_current() -> recent_cpu = add_mixed(thread_current() -> recent_cpu, 1);

		//매 4틱마다, 모든 스레드의 우선순위를 다시 계산해주기
		if (ticks % 4 == 0)
		{
			// for (struct list_elem *e = all_list_front(); e != all_list_end(); e = list_next(e))
			// {
			// 	struct thread * temp = list_entry(e, struct thread, a_elem);
			// 	//priority = PRI_MAX - (recent_cpu / 4) - (nice * 2)
			// 	mlfqs_calculate_priority(temp);
			// }
			mlfqs_recalculate_priority();
		
	
			//매 초마다, 모든 스레드의 recent_cpu 업데이트하기
			//1초는 몇 틱으로 구성되어 있는지?
			//timer.h에 TIMER_FREQ 매크로 상수로 선언되어 있음
			if (ticks % TIMER_FREQ == 0)
			{
				//load_avg를 먼저 계산해준 후 recent_cpu계산 ?
				//mlfqs_calculate_load_avg();
				// for (struct list_elem *e = all_list_front(); e != all_list_end(); )
				// {
				// 	struct thread * temp = list_entry(e, struct thread, a_elem);
				// 	//recent_cpu = decay * recent_cpu + nice
				// 	mlfqs_calculate_recent_cpu(temp);
				// 	e = list_next(e);
				// }
				mlfqs_recalculate_recent_cpu();
				mlfqs_calculate_load_avg();
			}
		}

	}

	#ifndef DEBUG_MLFQS
    /* 현재 tick이 global_tick 이상이면 슬립 리스트 확인 */
   if (get_global_tick() <= ticks) {
        thread_wake(ticks);
   }
   #endif
   #ifdef DEBUG_MLFQS
   	if (get_next_tick_to_awake() <= ticks)
	{
	thread_wake(ticks);
	}
	#endif
}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops (unsigned loops) {
	/* Wait for a timer tick. */
	int64_t start = ticks;
	while (ticks == start)
		barrier ();

	/* Run LOOPS loops. */
	start = ticks;
	busy_wait (loops);

	/* If the tick count changed, we iterated too long. */
	barrier ();
	return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait (int64_t loops) {
	while (loops-- > 0)
		barrier ();
}

/* Sleep for approximately NUM/DENOM seconds. */
static void
real_time_sleep (int64_t num, int32_t denom) {
	/* Convert NUM/DENOM seconds into timer ticks, rounding down.

	   (NUM / DENOM) s
	   ---------------------- = NUM * TIMER_FREQ / DENOM ticks.
	   1 s / TIMER_FREQ ticks
	   */
	int64_t ticks = num * TIMER_FREQ / denom;

	ASSERT (intr_get_level () == INTR_ON);
	if (ticks > 0) {
		/* We're waiting for at least one full timer tick.  Use
		   timer_sleep() because it will yield the CPU to other
		   processes. */
		timer_sleep (ticks);
	} else {
		/* Otherwise, use a busy-wait loop for more accurate
		   sub-tick timing.  We scale the numerator and denominator
		   down by 1000 to avoid the possibility of overflow. */
		ASSERT (denom % 1000 == 0);
		busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000));
	}
}
