/*****************************************************************************
* Product: DPP example
* Last Updated for Version: 5.9.7
* Date of the Last Update:  2017-08-20
*
*                    Q u a n t u m     L e a P s
*                    ---------------------------
*                    innovating embedded systems
*
* Copyright (C) Quantum Leaps, LLC. All rights reserved.
*
* This program is open source software: you can redistribute it and/or
* modify it under the terms of the GNU General Public License as published
* by the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* Alternatively, this program may be distributed and modified under the
* terms of Quantum Leaps commercial licenses, which expressly supersede
* the GNU General Public License and are specifically designed for
* licensees interested in retaining the proprietary status of their code.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*
* Contact information:
* https://state-machine.com
* mailto:info@state-machine.com
*****************************************************************************/
#include "qpc.h"
#include "dpp.h"
#include "bsp.h"

/* local "extended" thread object ..........................................*/
static QXThread l_test1;
static QXThread l_test2;
static QXMutex l_mutex;
static QXSemaphore l_sema;

/* Thread-Local Storage for the "extended" threads .........................*/
typedef struct {
    uint32_t foo;
    uint8_t bar[10];
} TLS_test;
static TLS_test l_tls1;
static TLS_test l_tls2;

static void lib_fun(uint32_t x) {
    QXK_TLS(TLS_test *)->foo = x;
}

/* global pointer to the test thread .......................................*/
QXThread * const XT_Test1 = &l_test1;
QXThread * const XT_Test2 = &l_test2;

/*..........................................................................*/
static void Thread1_run(QXThread * const me) {

    me->super.thread = &l_tls1; /* initialize the TLS for Thread1 */

    QXMutex_init(&l_mutex, 3U);

    (void)me;
    for (;;) {
        float volatile x;

        /* wait on a semaphore (BLOCK with timeout) */
        (void)QXSemaphore_wait(&l_sema, BSP_TICKS_PER_SEC);
        BSP_ledOn();

        QXMutex_lock(&l_mutex, QXTHREAD_NO_TIMEOUT); /* <== LOCK */
        /* some flating point code to exercise the VFP... */
        x = 1.4142135F;
        x = x * 1.4142135F;
        QXThread_delay(1U); /* blocking while holding a mutex (ALLOWED) */
        QXMutex_unlock(&l_mutex); /* <== UNLOCK */

        QXThread_delay(BSP_TICKS_PER_SEC/7);  /* BLOCK */

        /* publish to thread2 */
        QF_PUBLISH(Q_NEW(QEvt, TEST_SIG), &l_test1);

        /* test TLS */
        lib_fun(1U);
    }
}

/*..........................................................................*/
void Test1_ctor(void) {
    QXThread_ctor(&l_test1, Q_XTHREAD_CAST(&Thread1_run), 0U);
}

/*..........................................................................*/
static void Thread2_run(QXThread * const me) {

    me->super.thread = &l_tls2; /* initialize the TLS for Thread2 */

    /* subscribe to the test signal */
    QActive_subscribe(&me->super, TEST_SIG);

    /* initialize the semaphore before using it
    * NOTE: the semaphore is initialized in the highest-priority thread
    * that uses it. Alternatively, the semaphore can be initialized
    * before any thread runs.
    */
    QXSemaphore_init(&l_sema,
                     0U,  /* count==0 (signaling semaphore) */
                     1U); /* max_count==1 (binary semaphore) */

    for (;;) {
        QEvt const *e;

        /* some flating point code to exercise the VFP... */
        float volatile x;
        x = 1.4142135F;
        x = x * 1.4142135F;

        /* wait on the internal event queue (BLOCK) with timeout */
        e = QXThread_queueGet(BSP_TICKS_PER_SEC/2);
        BSP_ledOff();

        if (e != (QEvt *)0) { /* event actually delivered? */
            QF_gc(e); /* recycle the event manually! */
        }
        else {
            QXThread_delay(BSP_TICKS_PER_SEC/2);  /* wait more (BLOCK) */
            QXSemaphore_signal(&l_sema); /* signal Thread1 */
        }

        /* test TLS */
        lib_fun(2U);
    }
}

/*..........................................................................*/
void Test2_ctor(void) {
    QXThread_ctor(&l_test2, Q_XTHREAD_CAST(&Thread2_run), 0U);
}
