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
        // memory_order_acquire on success due to subsequent exclusive reads must happen after this read
        //      and also the following m_shared_counter read.
        while (!m_exclusive_access.compare_exchange_weak(expected, true, memory_order_acquire, memory_order_relaxed));
        {
            expected = false;
        }

        //
        // Wait for all current shared accesses to exit.
        //
        // Q. How do I make sure any write following this exclusive lock will not be sequenced before it?
        //      Do I have to use memory_order_seq_cst here for that to happen.
        int ctr = -1;
        while ((ctr = m_shared_counter.load(memory_order_relaxed)) != 0)
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
        // memory_order_acquire on success due to subsequent m_exclusive_access read must happen after this increment.
        while (!m_shared_counter.compare_exchange_weak(current_ctr, current_ctr + 1, memory_order_acquire, memory_order_relaxed));

        // memory_order_acquire due to subsequent shared reads must happen after this read
        //      and also the following memory_order_relaxed reads.
        if (m_exclusive_access.load(memory_order_acquire))
        {
            //
            // decrement shared counter.
            // This is required so that the next wait may not cause a deadlock.
            //
            current_ctr++;
            // memory_order_acquire on success due to subsequent m_exclusive_access read loop must happen after this decrement.
            while (!m_shared_counter.compare_exchange_weak(current_ctr, current_ctr - 1, memory_order_acquire, memory_order_relaxed));

            // Wait until current exclusive access has exited.
            while (m_exclusive_access.load(memory_order_relaxed));

            // repeat attempt to shared lock.
            lock_shared();
        }
    }

    // to exit exclusive access.
    void unlock()
    {
        // memory_order_release due to all exclusive access writes issued before this must happen before this write.
        m_exclusive_access.store(false, memory_order_release);
    }

    // to exit shared access.
    void unlock_shared()
    {
        //
        // decrement shared counter.
        //
        int current_ctr = m_shared_counter.load(memory_order_relaxed);
        // memory_order_acquire on success due to any subsequent m_shared_counter read must happen after this decrement.
        while (!m_shared_counter.compare_exchange_weak(current_ctr, current_ctr - 1, memory_order_acquire, memory_order_relaxed));
    }

private:
    atomic<int> m_shared_counter;
    atomic<bool> m_exclusive_access;
};


}
