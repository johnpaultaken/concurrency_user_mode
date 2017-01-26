#pragma once

#include <iostream>
#include <atomic>

using std::cerr;
using std::memory_order_relaxed;
using std::memory_order_consume;
using std::memory_order_release;

// Lock free stack implementation in C++11.
// Stack can grow in size unbounded.
// You must use a per-thread arena malloc implementation like tcmalloc or recent libc malloc.
/*
Problems identified:
1. compare_exchange checks a pointer to node to make sure head has not changed. 
    But there is a remote chance the node could have been freed, reallocated, and placed 
    at head by other threads in the mean time.
    This would be a problem for pop because the new head could be incorrect.
2. Even in a malloc implementation with per-thread arenas, if one thread frees node allocated
    by a different thread, does that cause a lock to happen ?
    I saw a stackoverflow posting that each arena has a thread-specific garbage list.
*/
template<typename T>
class stack_lf
{
public:
    stack_lf() : m_top(nullptr)
    {
        if (!m_top.is_lock_free())
        {
            cerr << "\nFalling back to lock based implementation of stack_lf.";
        }
    }

    void push(const T & item)
    {
        auto newtop = new node(item);
        // memory_order_relaxed due to no following dereferencing of top.
        auto top = m_top.load(memory_order_relaxed);
        do
        {
            newtop->m_previous = top;
        } while (!m_top.compare_exchange_weak(top, newtop, memory_order_release, memory_order_relaxed));
    }

    // Faster memory_order_consume can be used instead of memory_order_release in pop() due to dependent load.
    bool pop(T &item)
    {
        // memory_order_consume due to following operation top->m_previous.
        auto top = m_top.load(memory_order_consume);

        // memory_order_consume on failure due to following operation top->m_previous.
        // memory_order_consume on success due to following operation top->m_item.
        while (top && (!m_top.compare_exchange_weak(top, top->m_previous, memory_order_consume, memory_order_consume)));

        if (top)
        {
            item = top->m_item;
            delete top;
            return true;
        }
        else
        {
            return false;
        }
    }

private:
    struct node
    {
        node(const T & item) : m_item(item)
        {
        }
        node * m_previous;
        T m_item;
    };

    std::atomic<node *> m_top;
};
