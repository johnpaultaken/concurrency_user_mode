#pragma once

#include <iostream>
#include <atomic>
#include <stdexcept>

using std::cerr;
using std::memory_order_relaxed;
using std::memory_order_consume;
using std::memory_order_acquire;
using std::memory_order_release;
using std::memory_order_seq_cst;

using std::atomic_flag;
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
    shared_mutex() : m_shared_counter{ 0 }, m_exclusive_access{ false }
    {
    }

    // to request exclusive access.
    void lock()
    {
        // announce intention for exclusive access.
        // wait if another exclusive access is already in process.
        bool expected = false;
        // memory_order_acquire on success due to following m_shared_counter read must be sequenced after this read.
        while (!m_exclusive_access.compare_exchange_weak(expected, true, memory_order_acquire, memory_order_relaxed));
        {
            expected = false;
        }

        // wait for all current shared accesses to exit.
        // memory_order_seq_cst due to following exclusive access is expected to write that must not be sequenced before this read.
        //      TODO: Not sure about memory_order_seq_cst. memory_order_relaxed may be fine here. 
        int ctr = -1;
        while ((ctr = m_shared_counter.load(memory_order_seq_cst)) != 0)
        {
            if (ctr < 0)
            {
                throw std::logic_error("shared access counter has gone below zero.");
            }
        }
    }

    // to request shared access.
    void lock_shared()
    {
        increment_shared_counter();

        if (m_exclusive_access.load())
    }

private:
    atomic<int> m_shared_counter;
    atomic<bool> m_exclusive_access;
};


#ifdef JUNK
template<typename T>
class queue
{
public:
    queue(unsigned int initial_capacity = 64): m_freeList(initial_capacity)
    {
    }

    ~queue()
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

        m_pushList.push(pNode);
    }

    // Copies T on return. This allows queue to manage of its own internal storage.
    bool pop(T &item)
    {
        node * pNode = nullptr;
        if (!(pNode = m_popList.pop()))
        {
            // Acquire refillLock. 
            // Note:  This is not a system call lock. This is a 'lock-free' compare and swap operation.
            //             Using spinlock avoids any system call latency because expected spin is shorter than system call latency.
            while (m_refillLock.test_and_set(memory_order_acquire));

            // A refill might have happened by the time refillLock was acquired.
            // So try pop again.
            if (!(pNode = m_popList.pop()))
            {
                if (pNode = m_pushList.move())
                {
                    // reverse list.
                    pNode = reverseList(pNode);

                    auto refillList = pNode->pPrevious;
                    if (refillList)
                    {
                        m_popList.refill(refillList);
                    }
                }
            }

            // Release refillLock.
            // Using acquire/ release memory order for refillLock to ensure proper sequencing with popList access.
            m_refillLock.clear(memory_order_release);
        }
        
        if (pNode)
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

    // Reverses a node list.
    // argument pNode must not be nullptr.
    node * reverseList(node *pNode)
    {
        node * next = nullptr;
        auto current = pNode;

        do
        {
            auto previous = current->pPrevious;
            current->pPrevious = next;

            next = current;
            current = previous;
        } while (current);

        return next;
    }

    //
    // A list of nodes you can push to.
    //
    class node_push_list
    {
    public:
        node_push_list() : m_top{nullptr}
        {
        }

        void push(node * pNode)
        {
            auto newtop = pNode;

            // memory_order_relaxed due to no following dereferencing of top.
            auto top = m_top.load(memory_order_relaxed);

            do
            {
                newtop->pPrevious = top;
            } while (!m_top.compare_exchange_weak(top, newtop, memory_order_release, memory_order_relaxed));
            // memory_order_release on success due to node need to be pop ready for another thread.
            // memory_order_relaxed on failure due to no following dereferencing of top.
        }

        node * move()
        {
            // memory_order_consume due to this operation being equivalent to pop all.
            return m_top.exchange(nullptr, memory_order_consume);
        }

    private:
        std::atomic<node *> m_top;
    };

    //
    // A list of nodes you can pop from.
    //
    class node_pop_list
    {
    public:
        node_pop_list() : m_top{ nullptr }
        {
        }

        // Make sure at the time refill() is called list is empty.
        // Make sure only one thread calls refill() at a time.
        void refill(node * pNode)
        {
            // memory_order_relaxed due to no following dereferencing of top.
            auto top = m_top.load(memory_order_relaxed);

            if (top.pNode)
            {
                throw std::logic_error("refill() called when list not empty.");
            }

            head newtop;
            newtop.pNode = pNode;
            newtop.seqNum = top.seqNum + 1;

            // memory_order_release on success due to node need to be pop ready for another thread.
            // memory_order_relaxed on failure due to no following dereferencing of top.
            if (!m_top.compare_exchange_strong(top, newtop, memory_order_release, memory_order_relaxed))
            {
                throw std::logic_error("refill() called by more than one thread at a time.");
            }
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
            } while (top.pNode && (!m_top.compare_exchange_weak(top, newtop, memory_order_relaxed, memory_order_consume)));
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

            head(node * node) : pNode(node), seqNum(0)
            {
            }

            // for default initialization.
            head()
            {
            }
        };

        std::atomic<head> m_top;
    };

    //
    // A list of nodes you can both push to and pop from.
    //
    class node_list
    {
    public:
        node_list() : m_top{ nullptr }
        {
            if (!m_top.is_lock_free())
            {
                cerr << "\nFalling back to lock based implementation of lockfree::queue.";
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
            } while (top.pNode && (!m_top.compare_exchange_weak(top, newtop, memory_order_relaxed, memory_order_consume)));
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

            head(node * node) : pNode(node), seqNum(0)
            {
            }

            // for default initialization.
            head()
            {
            }
        };

        std::atomic<head> m_top;
    };

    //
    // A list of free nodes available to be used during push.
    // Popped nodes are recycled to this list to be used again for push, 
    // thus avoiding the memory allocator until a capacity increase.
    //
    // Note: This wrapper class on node_list makes sure pop() always returns a free node
    //  by doing memory allocation if node_list goes empty.
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
    node_push_list m_pushList;
    node_pop_list m_popList;

    std::atomic_flag m_refillLock = ATOMIC_FLAG_INIT;
};
#endif //JUNK

}
