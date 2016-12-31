
#include <iostream>
#include <atomic>
#include <future>
#include <vector>

using namespace std;
// Lock free stack implementation in C++11.
// Stack can grow in size unbounded.
// You must use a per-thread arena malloc implementation like tcmalloc or recent libc malloc.
/*
Problems identified:
1. compare_exchange checks a pointer to node to make sure head has not changed. 
    But there is a remote chance the node could have been freed, reallocated, and placed 
    at head by other threads in the mean time.
    This would be a problem for both push and pop.
2. In a malloc implementation with per-thread arenas, if one thread frees buffer allocated
    by a different thread, does that cause a lock to happen ?
    I saw a stackoverflow idea that each arena has a thread-specific garbage list.
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

    atomic<node *> m_top;
};

template<typename T>
void print(stack_lf<T> & s)
{
    T i{};
    while (s.pop(i))
    {
        cout << i << ' ';
    }
}

int main(int argc, char ** argv)
{
    stack_lf<int> slf;
    stack_lf<int> result;

    {
        vector<future<void>> vf;

        for (int c = 0; c < 10; c+=3)
        {
            vf.emplace_back(async([&slf, c]() {slf.push(c+1); }));
            vf.emplace_back(async([&slf, c]() {slf.push(c+2); }));
            vf.emplace_back(async([&slf, c]() {slf.push(c+3); }));
            vf.emplace_back(async([&slf, &result]() {int ip = 0; if (slf.pop(ip)) result.push(ip); }));
            vf.emplace_back(async([&slf, &result]() {int ip = 0; if (slf.pop(ip)) result.push(ip); }));
            vf.emplace_back(async([&slf, &result]() {int ip = 0; if (slf.pop(ip)) result.push(ip); }));
        }
    }

    int i = 0;
    while (slf.pop(i))
    {
        result.push(i);
    }
    print(result);

    cout << "\ndone" << flush;
    getchar();
    return 0;
}
