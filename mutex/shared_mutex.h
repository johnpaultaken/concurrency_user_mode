//----------------------------------------------------------------------------
// year   : 2017
// author : John Paul
// email  : johnpaultaken@gmail.com
//----------------------------------------------------------------------------

#pragma once

#include <iostream>
#include <atomic>
#include <stdexcept>

using std::cerr;
using std::memory_order_relaxed;
using std::memory_order_acquire;
using std::memory_order_release;

using std::atomic;

// Lock free shared_mutex implementation using C++11.
// Note: This can be used for a ReadersWriterLock functionality and is lock free.

// To build using gcc need the following options
//      -std=c++11 -pthread

/*
Notes:
This implementation uses lock free atomic operations compare and swap, swap, load, store.
The interface adheres to the C++17 shared_mutex interface.
*/

/*
Design:
Unlike implementation in shared_mutex_1 which uses two atomic variables,
here we just use one, so that the gap between accessing the two variables
is no longer a problem.

A counter keeps track of all accesses.
Counter is incremented before each shared access, and decremented after it.
When entering shared or exclusive access counter must not be already negative.
This means counter must remain negative during exclusive access.
To ensure this, before exclusive access, counter's value is swapped with -1
current_value = counter.swap(-1)
Exclusive access then waits until counter's value becomes -current_value-1
After exclusive access counter is set to 0.
*/

namespace lockfree
{

class shared_mutex
{
public:
    shared_mutex() : m_counter{ 0 }
    {
        if (!m_counter.is_lock_free())
        {
            cerr << "\nFalling back to lock based implementation of lockfree::shared_mutex.";
        }
    }

    // to enter exclusive access.
    void lock()
    {
        //
        // swap counter with -1, if not already negative.
        //
        int current_ctr = 0;
        while (!m_counter.compare_exchange_weak(current_ctr, -1, memory_order_relaxed, memory_order_relaxed))
        {
            current_ctr = (current_ctr < 0) ? 0 : current_ctr;
        }

        //
        // Wait until all ongoing shared accesses has exited.
        //
        // memory_order_acquire due to all PD reads issued after this read must 'happen after' this read
        // PD is data sturcture protected by using this shared_mutex.
        // Q. How do I make sure any PD write issued after this read 'happens after' this read?
        //    I believe since any PD write is going to be 'dependent' on a PD read before it,
        //    the correct memory order will naturally happen.
        int ctr = 0;
        while ((ctr = m_counter.load(memory_order_acquire)) != (-current_ctr - 1))
        {
            if (ctr < (-current_ctr - 1))
            {
                throw std::logic_error("counter has gone below expected.");
            }
        }
    }

    // to enter shared access.
    void lock_shared()
    {
        //
        // increment counter, if not already negative.
        //
        int current_ctr = 0;
        // memory_order_acquire on success due to all PD reads issued after this read must 'happen after' this read.
        // PD is data structure protected by using this shared_mutex.
        while (!m_counter.compare_exchange_weak(current_ctr, current_ctr + 1, memory_order_acquire, memory_order_relaxed))
        {
            current_ctr = (current_ctr < 0) ? 0 : current_ctr;
        }
    }

    // to exit exclusive access.
    void unlock()
    {
        // memory_order_release due to all PD writes issued before this write must 'happen before' this write.
        // PD is data structure protected by using this shared_mutex.
        m_counter.store(0, memory_order_release);
    }

    // to exit shared access.
    void unlock_shared()
    {
        //
        // decrement shared counter.
        //
        int current_ctr = m_counter.load(memory_order_relaxed);
        // Since there is no PD write issued before this write, memory_order_release is not needed here.
        while (!m_counter.compare_exchange_weak(current_ctr, current_ctr - 1, memory_order_relaxed, memory_order_relaxed));
    }

private:
    atomic<int> m_counter;
};


}
