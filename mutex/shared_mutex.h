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
namespace lockfree
{

class shared_mutex
{
public:
    shared_mutex() : m_shared_counter{ 0 }, m_exclusive_access{ false }
    {
        if (!m_shared_counter.is_lock_free())
        {
            cerr << "\nFalling back to lock based implementation of lockfree::shared_mutex.";
        }
    }

    // to enter exclusive access.
    void lock()
    {
        //
        // Wait until any current exclusive access has exited.
        // Then claim exclusive access which prevents any new shared or exclusive access.
        //
        bool expected = false;
        // memory_order_acquire on success due to m_shared_counter read issued after this read must 'happen after' this read
        while (!m_exclusive_access.compare_exchange_weak(expected, true, memory_order_acquire, memory_order_relaxed));
        {
            expected = false;
        }

        //
        // Wait for all current shared accesses to exit.
        //
        int ctr = -1;
        // memory_order_acquire due to all PD reads issued after this read must 'happen after' this read
        // PD is data sturcture protected by using this shared_mutex.
        // Q. How do I make sure any PD write issued after this read 'happens after' this read?
        //    I believe since any PD write is going to be 'dependent' on a PD read before it,
        //    the correct memory order will naturally happen.
        while ((ctr = m_shared_counter.load(memory_order_acquire)) != 0)
        {
            if (ctr < 0)
            {
                throw std::logic_error("shared access counter has gone below zero.");
            }
        }
    }

    // to enter shared access.
    void lock_shared()
    {
        //
        // increment shared counter
        //
        int current_ctr = m_shared_counter.load(memory_order_relaxed);
        // memory_order_acquire on success due to m_exclusive_access read issued after this read must 'happen after' this read.
        // The m_exclusive_access read will also 'happen after' this write because this read-write is atomic.
        while (!m_shared_counter.compare_exchange_weak(current_ctr, current_ctr + 1, memory_order_acquire, memory_order_relaxed));

        // memory_order_acquire due to all PD reads issued after this read must 'happen after' this read.
        // PD is data structure protected by using this shared_mutex.
        if (m_exclusive_access.load(memory_order_acquire))
        {
            //
            // decrement shared counter.
            // This is required so that the next wait may not cause a deadlock.
            //
            current_ctr++;
            // This m_shared_counter read 'happens after' previous m_exclusive_access read due to its memory_order_acquire.
            // However memory_order_acquire is needed here for properly ordering the following m_exclusive_access read.
            while (!m_shared_counter.compare_exchange_weak(current_ctr, current_ctr - 1, memory_order_acquire, memory_order_relaxed));

            // This m_exclusive_access read must happen after previous m_shared_counter read due to its memory_order_acquire.
            // However memory_order_acquire is needed here for properly ordering the following m_shared_counter read that happens due to recursive call.
            while (m_exclusive_access.load(memory_order_acquire));

            // repeat attempt to shared lock.
            lock_shared();
        }
    }

    // to exit exclusive access.
    void unlock()
    {
        // memory_order_release due to all PD writes issued before this write must 'happen before' this write.
        // PD is data structure protected by using this shared_mutex.
        m_exclusive_access.store(false, memory_order_release);
    }

    // to exit shared access.
    void unlock_shared()
    {
        //
        // decrement shared counter.
        //
        int current_ctr = m_shared_counter.load(memory_order_relaxed);
        // Since there is no PD write issued before this write, memory_order_release is not needed here.
        // However memory_order_acquire may be needed on success to ensure a read of m_exclusive_access issued after this read 'happens after' this read.
        // This could happen for example if this thread calls lock() after exiting this function.
        while (!m_shared_counter.compare_exchange_weak(current_ctr, current_ctr - 1, memory_order_acquire, memory_order_relaxed));
    }

private:
    atomic<int> m_shared_counter;
    atomic<bool> m_exclusive_access;
};


}
