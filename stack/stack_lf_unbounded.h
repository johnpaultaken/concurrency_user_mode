#pragma once

#include <iostream>
#include <atomic>

using std::cerr;
using std::memory_order_relaxed;
using std::memory_order_consume;
using std::memory_order_release;

// Lock free stack implementation in C++11.
// Stack can grow in size unbounded.
/*
Notes:
This implementation removes the restriction that you must use a per-thread arena allocator.
This implementation also fixes the two problems identified in stack_lf_unbounded_pta.h
Problem 1 is fixed by also adding a sequence number to atomic top variable.
Problem 2 is fixed by removing per-thread arena allocator restriction.

Other notes:
1. Cannot use a preallocated array as storage for stack elements because
    say for push,
    copy element and then atomic push will not work because copy can race,
    atomic push and then copy element will not work because pop can happen while copy.
2. Since the nodes don't get deleted at pop, but goes to a free list, we must call
    the element destructor manually. This is important if the stack is used to store
    smart pointers since the stack must not hold references to elements after pop.
3. Since the nodes don't always get allocated at push, we must do in-place copy
    construction manually. We cannot use assignment because assignment needs a
    previously constructed object.
*/
namespace lockfree
{

template<typename T>
class stack
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
        node * pPrevious;
        T item;
    };

    class node_stack
    {
    public:
        node_stack() : m_top{nullptr,0}
        {
            if (!m_top.is_lock_free())
            {
                cerr << "\nFalling back to lock based implementation of lockfree::stack.";
            }
        }

        void push(node * pNode)
        {
            // memory_order_relaxed due to node_stack dealing only node pointer, not node contents.
            auto top = m_top.load(memory_order_relaxed);
            do
            {
                pNode->pPrevious = top.pNode;
            } while (!m_top.compare_exchange_weak(top, newtop, memory_order_relaxed, memory_order_relaxed));
        }
    private:
        struct head
        {
            node * pNode;
            unsigned int seqNum;
        };

        std::atomic<head> m_top;
    };

};

}
