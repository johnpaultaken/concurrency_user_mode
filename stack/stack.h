//----------------------------------------------------------------------------
// year   : 2017
// author : John Paul
// email  : johnpaultaken@gmail.com
//----------------------------------------------------------------------------

#pragma once

#include <iostream>
#include <atomic>

using std::cerr;
using std::memory_order_relaxed;
using std::memory_order_consume;
using std::memory_order_release;

// Lock free stack implementation in C++11.
// Stack can grow in size unbounded.

// To build using gcc need the following options
//      -std=c++11 -pthread -march=native
//      arch option is needed because not all architectures support 16 byte atomic.

/*
Notes:
This implementation removes the restriction that you must use a per-thread arena allocator.
This implementation also fixes the two problems identified in stack_lf_unbounded_pta.h
Problem 1 is fixed by also adding a sequence number to atomic top variable which is incremented on push.
Problem 2 is fixed by removing per-thread arena allocator restriction by using an internal free list for allocation.

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
    stack(unsigned int initial_capacity = 64): m_freeList(initial_capacity)
    {
    }

    ~stack()
    {
        T tempObj;
        while (pop(tempObj));
    }

    // Makes a copy of T internally. This allows proper object lifetime management. T can also be a smart pointer.
    void push(const T & item)
    {
        auto pNode = m_freeList.pop();

        // in-place copy construction
        new (&(pNode->item)) T(item);

        m_occupiedList.push(pNode);
    }

    // Copies T on return. This allows stack management of its own internal storage.
    bool pop(T &item)
    {
        node * pNode;
        if (pNode = m_occupiedList.pop())
        {
            // copy to client.
            item = pNode->item;

            // destruct internal copy without freeing memory.
            pNode->item.~T();

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

    class node_list
    {
    public:
        node_list() : m_top{nullptr}
        {
            if (!m_top.is_lock_free())
            {
                cerr << "\nFalling back to lock based implementation of lockfree::stack.";
            }
        }

        void push(node * pNode)
        {
            // memory_order_relaxed due to no following dereferencing of top.
            auto top = m_top.load(memory_order_relaxed);

            head newtop;
            newtop.pNode = pNode;

            do
            {
                pNode->pPrevious = top.pNode;
                newtop.seqNum = top.seqNum + 1;
            } while (!m_top.compare_exchange_weak(top, newtop, memory_order_release, memory_order_relaxed));
            // memory_order_release on success due to node need to be pop ready for another thread.
            // memory_order_relaxed on failure due to no following dereferencing of top.
        }

        node * pop()
        {
            // memory_order_consume due to following dependent load operation top.pNode->pPrevious.
            auto top = m_top.load(memory_order_consume);

            head newtop;
            
            do
            {
                if (top.pNode)
                {
                    newtop.pNode = top.pNode->pPrevious;
                    newtop.seqNum = top.seqNum;
                }
            }
            while (top.pNode && (!m_top.compare_exchange_weak(top, newtop, memory_order_relaxed, memory_order_consume)));
            // memory_order_consume on failure due to following dependent load operation top.pNode->pPrevious.
            //      Note: Dependent load allows faster memory_order_consume to be used instead of memory_order_release.
            // memory_order_relaxed on success due to top actually read by the previous atomic operation, not the current one.
            //      Note: However success cannot specify weaker ordering than failure until C++17.

            return top.pNode;
        }

    private:
        struct head
        {
            node * pNode;
            unsigned int seqNum;

            head(node * node): pNode(node), seqNum(0)
            {
            }

            // for default initialization.
            head()
            {
            }
        };

        std::atomic<head> m_top;
    };

    // This class makes sure pop() always returns a node.
    // If node_list is empty on pop(), node is allocated and inserted into the list.
    class free_list: public node_list
    {
    public:
        free_list(unsigned int initial_capacity)
        {
            for (unsigned int i = 0; i < initial_capacity; ++i)
            {
                // There is no need to call constructor here. So just use malloc.
                node_list::push(static_cast<node *>(malloc(sizeof(node))));
            }
        }

        ~free_list()
        {
            node * pNode = nullptr;

            while (pNode = node_list::pop())
            {
                free(pNode);
            }
        }

        node * pop()
        {
            node * ret = nullptr;

            while (!(ret = node_list::pop()))
            {
                // There is no need to call constructor here. So just use malloc.
                node_list::push(static_cast<node *>(malloc(sizeof(node))));
            }

            return ret;
        }
    };

    free_list m_freeList;
    node_list m_occupiedList;
};

}
