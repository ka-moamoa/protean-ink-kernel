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
 * thread.c
 *
 *  Created on: 11 Feb 2018
 *
 */

#include "ink.h"

extern uint32_t nvmBuffStartAddr;
extern uint32_t nvmBuffEndAddr;

// indicates if devices rebooted
uint8_t __booted = 1;

void copy_buff_fram_to_sram(buffer_t *buffer)
{
    if (__booted) 
    { // get data from FRAM only if the device reboots, otherwise use SRAM copy
        uint32_t buffSize = (((uint32_t)(&nvmBuffEndAddr)) - (((uint32_t)(&nvmBuffStartAddr)) + sizeof(nvmBuffEndAddr))) / 2;
        if (!(buffer->idx))
        {
            void *nvm_buffer = (void *)(((uint32_t)(&nvmBuffStartAddr)) + sizeof(nvmBuffStartAddr)); // pointer to the first buffer
            __NVM_MEMSET(buffer->buf, nvm_buffer, buffSize); // copy ٖfirst FRAM buffer to SRAM buffer
            KERNEL_LOG_DEBUG("buff[0] -> SRAM");
        }
        else
        {

            void *nvm_buffer = (void *)(((uint32_t)(&nvmBuffStartAddr)) + sizeof(nvmBuffStartAddr) + buffSize); // pointer to the second buffer
            __NVM_MEMSET(buffer->buf, nvm_buffer, buffSize); // copy second FRAM buffer to SRAM buffer
            KERNEL_LOG_DEBUG("buff[1] -> SRAM");
        }
        __booted = 0;
    }
}

void copy_buff_sram_to_fram(buffer_t *buffer)
{
    void *buf = buffer->buf;
    if (!(buffer->_idx ^ 1))
    {
        void *nvm_buffer = (void *)(((uint32_t)(&nvmBuffStartAddr)) + sizeof(nvmBuffStartAddr)); // pointer to the first buffer
        __NVM_MEMSET(nvm_buffer, buf, buffer->size);  // copy SRAM buffer to first FRAM buffer
        KERNEL_LOG_DEBUG("SRAM -> buff[0]");
    }
    else
    {
        void *nvm_buffer = (void *)(((uint32_t)(&nvmBuffStartAddr)) + sizeof(nvmBuffStartAddr) + buffer->size); // pointer to the second buffer
        __NVM_MEMSET(nvm_buffer, buf, buffer->size);  // copy SRAM buffer to second FRAM buffer
        KERNEL_LOG_DEBUG("SRAM -> buff[1]");
    }
}

// prepares the stack of the thread for the task execution
static inline void __prologue(thread_t *thread)
{
    buffer_t *buffer = &thread->buffer;
    // copy original stack to the temporary stack
    // __dma_word_copy(buffer->buf[buffer->idx],buffer->buf[buffer->idx ^ 1], buffer->size>>1);

#if KERNEL_FRAM
    copy_buff_fram_to_sram(buffer);
    KERNEL_LOG_DEBUG("copy from FRAM done");

#else
    buffer->buf[buffer->idx] = buffer->buf[buffer->idx ^ 1];
#endif
}


// runs one task inside the current thread
void __tick(thread_t *thread)
{
    void *buf;
    switch (thread->state)
    {
    case TASK_READY:
        // refresh thread stack
        __prologue(thread);
        // get thread buffer
#if KERNEL_FRAM
        buf = thread->buffer.buf;
#else
        buf = thread->buffer.buf[thread->buffer._idx^1];
#endif

        // Check if it is the entry task. The entry task always
        // consumes an event in the event queue.
        if (thread->next == thread->entry)
        {
            // pop an event since the thread most probably woke up due to
            // an event
            isr_event_t *event = __lock_event(thread);
            // push event data to the entry task
            thread->next = (void *)((entry_task_t)thread->entry)(buf, (void *)event);
            // the event should be released (deleted)
#if KERNEL_FRAM
            copy_buff_sram_to_fram(&thread->buffer);
            KERNEL_LOG_DEBUG("copy to FRAM done");
#endif
            thread->state = TASK_RELEASE_EVENT;
        }
        else
        {
            thread->next = (void *)(((task_t)thread->next)(buf));
#if KERNEL_FRAM
            copy_buff_sram_to_fram(&thread->buffer);
            KERNEL_LOG_DEBUG("copy to FRAM done");
#endif
            thread->state = TASK_FINISHED;
            // break;
            goto TASK_FINISHED_STATE;
        }
    case TASK_RELEASE_EVENT:
        // release any event which is popped by the task
        __release_event(thread);
        thread->state = TASK_FINISHED;
    
TASK_FINISHED_STATE:
    case TASK_FINISHED:
        //switch stack index to commit changes
        thread->buffer._idx = thread->buffer.idx ^ 1;
        KERNEL_LOG_DEBUG("_idx -> idx^1");
        thread->state = TASK_COMMIT;
    case TASK_COMMIT:
        // copy the real index from temporary index
        thread->buffer.idx = thread->buffer._idx;
        KERNEL_LOG_DEBUG("idx -> _idx");

        // Task execution finished. Check if the whole tasks are executed (thread finished)
        if (thread->next == NULL)
        {
            __disable_irq();
            // check if there are any pending events
            if (!__has_events(thread))
            {
                // suspend the thread if there are no pending events
                __stop_thread(thread);
            }
            else
            {
                // thread re-starts from the entry task
                thread->next = thread->entry;
                // ready to execute tasks again.
                thread->state = TASK_READY;
            }
            __enable_irq();
        }
        else
        {
            // ready to execute successive tasks
            thread->state = TASK_READY;
        }
        break;
    default:
        KERNEL_LOG_WARNING("undefined task state");
        break;
    }
}


