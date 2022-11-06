// This file is part of InK.
// 
// author = "Kasım Sinan Yıldırım " 
// maintainer = "Kasım Sinan Yıldırım "
// email = "sinanyil81 [at] gmail.com" 
//  
// copyright = "Copyright 2018 Delft University of Technology" 
// license = "LGPL" 
// version = "3.0" 
// status = "Production"
//
// 
// InK is free software: you ca	n redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

/*
 * scheduler.c
 *
 *  Created on: 12 Feb 2018
 *
 */

#include "ink.h"
#include "scheduler.h"
#include "priority.h"

enum { SCHED_SELECT, SCHED_BUSY };

#if KERNEL_FRAM
// variables for keeping track of the ready threads
static __nv priority_t _priorities;
static __nv uint8_t _sched_state;

// all threads in the system
static __nv thread_t _threads[MAX_THREADS];
static thread_t _sram_threads[MAX_THREADS];

// the id of the current thread being executed.
static __nv thread_t *_thread;
static thread_t *_sram_thread = NULL;

#else
// variables for keeping track of the ready threads
static priority_t _priorities;
static volatile uint8_t _sched_state = SCHED_SELECT;

// all threads in the system
static thread_t _threads[MAX_THREADS];

// the id of the current thread being executed.
static thread_t *_thread = NULL;
#endif

void __scheduler_boot_init() {
    uint8_t i;

// clear priority variables for the threads
#if KERNEL_FRAM
    priority_t _sram_priorities;
    /* 
     * __scheduler_boot_init() is called on first boot only
     * therfore not reading _priorities before function call to reduce overhead
     */
    // __NVM_GET(_sram_priorities, _priorities);
    __priority_init(&_sram_priorities);
    __NVM_SET(_priorities, _sram_priorities);
    __NVM_SET(_sched_state, SCHED_SELECT);
    for (i = 0; i < MAX_THREADS; i++){
        // threads are not created yet
        __NVM_SET(_threads[i].state, THREAD_STOPPED);
        _sram_threads[i].state = THREAD_STOPPED;
    }
#else
    __priority_init(&_priorities);
    _sched_state = SCHED_SELECT;
    for (i = 0; i < MAX_THREADS; i++){
        // threads are not created yet
        _threads[i].state = THREAD_STOPPED;
    }
#endif
}

// Assigns a slot to a thread. Should be called ONLY at the first system boot
#if KERNEL_FRAM   
void __create_thread(uint8_t priority, void *entry, void *data, uint16_t size)
{ 
    // init properties
    __NVM_SET(_threads[priority].priority, priority);
    __NVM_SET(_threads[priority].entry, entry);
    __NVM_SET(_threads[priority].next, entry);
    __NVM_SET(_threads[priority].state, THREAD_STOPPED);

    // init shared buffer
    __NVM_SET(_threads[priority].buffer.buf, data);
    __NVM_SET(_threads[priority].buffer.idx, 0);
    __NVM_SET(_threads[priority].buffer.size, size);
}
#else
void __create_thread(uint8_t priority, void *entry, void *data_org,
                     void *data_temp, uint16_t size)
{
    // init properties
    _threads[priority].priority = priority;
    _threads[priority].entry = entry;
    _threads[priority].next = entry;
    _threads[priority].state = THREAD_STOPPED;

    // init shared buffer
    _threads[priority].buffer.buf[0] = data_org;
    _threads[priority].buffer.buf[1] = data_temp;
    _threads[priority].buffer.idx = 0;
    _threads[priority].buffer.size = size;
}
#endif

// puts the thread in waiting state
inline void __stop_thread(thread_t *thread){
#if KERNEL_FRAM
    priority_t _sram_priorities;
    __NVM_GET(_sram_priorities, _priorities);
    __priority_remove(thread->priority, &_sram_priorities);
    __NVM_SET(_priorities, _sram_priorities);
#else
    __priority_remove(thread->priority, &_priorities);
#endif
    thread->state = THREAD_STOPPED;
}

// puts the thread in waiting state
void __evict_thread(thread_t *thread){
#if KERNEL_FRAM
    priority_t _sram_priorities;
    __NVM_GET(_sram_priorities, _priorities);
    __priority_remove(thread->priority, &_sram_priorities);
    __NVM_SET(_priorities, _sram_priorities);
#else
    __priority_remove(thread->priority, &_priorities);
#endif
    thread->next = NULL;
    thread->state = THREAD_STOPPED;
}

void __set_sing_timer(thread_t *thread,uint16_t timing){
    thread->sing_timer = timing;
    return;
}

//TODO: update necessary
void __set_expr_timer(thread_t *thread,uint16_t timing){
    thread->expr_timer = timing;
    return;
}


void __set_pdc_timer(thread_t *thread,uint16_t timing){
    thread->pdc_timer = timing;
    return;
}

void __set_pdc_period(thread_t *thread,uint16_t period){
    thread->pdc_period = period;
    return;
}

uint16_t __get_pdc_timer(thread_t *thread){
    return thread->pdc_timer;
}

uint16_t __get_pdc_period(thread_t *thread){
    return thread->pdc_period;
}

// puts the thread in active state
inline void __start_thread(thread_t *thread) {
    thread->next = thread->entry;
#if KERNEL_FRAM
    priority_t _sram_priorities;
    __NVM_GET(_sram_priorities, _priorities);
    __priority_insert(thread->priority, &_sram_priorities);
    __NVM_SET(_priorities, _sram_priorities);
    __NVM_SET(_threads[thread->priority].state, TASK_READY);
#else
    __priority_insert(thread->priority, &_priorities);
#endif
    thread->state = TASK_READY;
}

// returns the highest-priority thread
static inline thread_t *__next_thread(){
#if KERNEL_FRAM
    priority_t _sram_priorities;
    __NVM_GET(_sram_priorities, _priorities);
    uint8_t idx = __priority_highest(&_sram_priorities);
    /* 
     * __priority_highest() is not modifying _sram_priorities
     * therefore not writing it back to NVM to reduce overhead
     */
    // __NVM_SET(_priorities, _sram_priorities);
    __NVM_GET(_sram_threads[idx], _threads[idx]);
    if(idx)
        return &_sram_threads[idx];
#else
    uint8_t idx = __priority_highest(&_priorities);
    if(idx)
        return &_threads[idx];
#endif

    return NULL;
}

inline thread_t *__get_thread(uint8_t priority){
#if KERNEL_FRAM
    __NVM_GET(_sram_threads[priority], _threads[priority]);
    return &_sram_threads[priority];
#else
    return &_threads[priority];
#endif
}

// finish the interrupted task before enabling interrupts
static inline void __task_commit(){
    KERNEL_LOG_DEBUG("in task_commit()");
#if KERNEL_FRAM
    __NVM_GET(_sram_threads, _threads);
    __NVM_GET(_sram_thread,_thread);
    if(_sram_thread){
        __tick(_sram_thread);
        KERNEL_LOG_DEBUG("done executing task");
        __NVM_SET(_thread, _sram_thread);
        KERNEL_LOG_DEBUG("updated _thread");
        __NVM_SET(_threads[_sram_thread->priority], *_sram_thread);
        KERNEL_LOG_DEBUG("updated _threads[priority]");
    }
#else
    if(_thread){
        __tick(_thread);
    }
#endif
    KERNEL_LOG_DEBUG("exiting task_commit");
}

// at each step, the scheduler selects the highest priority thread and
// runs the next task within the thread
void __scheduler_run()
{
    // For the sake of consistency, the event insertion by an ISR which
    // was interrupted by a power failure should be committed to the
    // event queue _events in isrmanager.c before enabling the interrupts.
    __events_commit();

    // always finalize the latest task before enabling interrupts since
    // this task might be interrupted by a power failure and the changes
    // it performs on the system variables (e.g. on _priorities due to
    // signaling another task or on the event queue _events in isrmanager.c)
    // will be committed before enabling interrupts so that these variables
    // remain consistent and stable.
    __task_commit();

    // __reboot_timers();
    
    // enable interrupts
    __enable_irq();

#if KERNEL_FRAM
    uint8_t _sram_sched_state;
    __NVM_GET(_sram_sched_state, _sched_state);
    while (1){
        switch (_sram_sched_state){
        case SCHED_SELECT:
            // the scheduler selects the highest priority task right
            // after it has finished the execution of a single task
            _sram_thread = __next_thread();
            KERNEL_LOG_DEBUG("getting next thread");
            __NVM_SET(_thread,_sram_thread);

            __NVM_SET(_sched_state, SCHED_BUSY);
            _sram_sched_state = SCHED_BUSY;
            
        case SCHED_BUSY:
            // always execute the selected task to completion
            // execute one task inside the highest priority thread
            __NVM_GET(_sram_thread,_thread);
            if (_sram_thread){
                __tick(_sram_thread);
                KERNEL_LOG_DEBUG("done executing task");
                __NVM_SET(_thread, _sram_thread)
                KERNEL_LOG_DEBUG("updated _thread");
                __NVM_SET(_threads[_sram_thread->priority], *_sram_thread);
                KERNEL_LOG_DEBUG("updated _threads[priority]");
                // after execution of one task, check the events
                __NVM_SET(_sched_state, SCHED_SELECT);
                _sram_sched_state = SCHED_SELECT;
                break;
            }
            __NVM_SET(_sched_state, SCHED_SELECT);
            _sram_sched_state = SCHED_SELECT;
            __disable_irq();
            // check the ready queue for the last time
            if(!__next_thread()){
                MXC_LP_EnterSleepMode();
                __enable_irq();
            }
        }
    }
#else
    while (1){
        switch (_sched_state){
        case SCHED_SELECT:
            // the scheduler selects the highest priority task right
            // after it has finished the execution of a single task
            _thread = __next_thread();
            _sched_state = SCHED_BUSY;
        case SCHED_BUSY:
            // always execute the selected task to completion
            // execute one task inside the highest priority thread
            if (_thread){
                __tick(_thread);
                // after execution of one task, check the events
                _sched_state = SCHED_SELECT;
                break;
            }
            _sched_state = SCHED_SELECT;
            __disable_irq();
            // check the ready queue for the last time
            if(!__next_thread()){
                MXC_LP_EnterSleepMode();
                __enable_irq();
            }
        }
    }
#endif
}

#if KERNEL_FRAM
void __initSchedulerGlobalVars() {
    __NVM_SET(_sched_state, SCHED_SELECT);
    __NVM_SET(_thread, NULL);
}
#endif