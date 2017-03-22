//
// use the following command line to build using gcc
// g++ -std=c++11 -pthread -march=native test_queue.cpp
//

#include "shared_mutex.h"

// This is used for the std::unique_lock wrapper only;
// which is used to wrap our lockfree::shared_mutex.
#include <mutex>
using std::unique_lock;

// This is used for the std::shared_lock wrapper only;
// which is used to wrap our lockfree::shared_mutex.
#include <shared_mutex>
using std::shared_lock;

#include <iostream>
#include <future>
#include <vector>
#include <set>
#include <random>

using std::cout;
using std::pair;
using std::make_pair;
using std::vector;
using std::logic_error;
using std::flush;

void testcase_sanity()
{
    lockfree::shared_mutex sm;

    sm.lock_shared();
    sm.lock_shared();
    sm.unlock_shared();
    sm.lock_shared();
    sm.unlock_shared();
    sm.unlock_shared();

    sm.lock();
    sm.unlock();

    cout << "\n success";
    cout << " test sanity: single threaded shared and exclusive locking.";
}


class linked_list_single_threaded
{
public:
    static const unsigned int parallelism = 999;
    static const unsigned int signature = 0x12345678;
    static const unsigned int badcode = 0xbadc0de;

    linked_list_single_threaded()
    {
        head.element = signature;
        head.next = nullptr;

        badnode.element = badcode;
        badnode.next = &badnode;    // to cause infinite looping.

        for (unsigned int i = 0; i < parallelism; ++i)
        {
            freelist[i].allocated = false;
            freelist[i].element = badcode;
            freelist[i].next = &badnode;
        }
    }

    void pop_back()
    {
        if (head.next)
        {
            auto newlast = &head;
            while (newlast->next->next)
            {
                newlast = newlast->next;
            }
            newlast->next->allocated = false;
            newlast->next->element = badcode;
            newlast->next->next = &badnode;

            newlast->next = nullptr;
        }
    }

    void push_back()
    {
        node * newnode = nullptr;
        for (unsigned int i = 0; i < parallelism; ++i)
        {
            if (! freelist[i].allocated)
            {
                newnode = &freelist[i];
                freelist[i].allocated = true;
                freelist[i].element = signature;
                freelist[i].next = nullptr;

                break;
            }
        }

        if (newnode)
        {
            auto currentlast = &head;
            while (currentlast->next)
            {
                currentlast = currentlast->next;
            }
            currentlast->next = newnode;
        }
    }

    pair<unsigned int, unsigned int> peek_back()
    {
        auto currentlast = &head;
        unsigned int numnodes = 1;
        while (currentlast->next)
        {
            currentlast = currentlast->next;
            numnodes++;
        }
        return make_pair(currentlast->element, numnodes);
    }

private:
    struct node
    {
        bool allocated;
        unsigned int element;
        node * next;
    };

    // nodes pre allocated to avoid locks in allocator.
    node freelist[parallelism];

    node badnode;

    node head;
};

template<typename linked_list>
void test_linked_list_single_threaded()
{
    linked_list list_st;

    static const unsigned int signature = linked_list::signature;

    vector<pair<unsigned int, unsigned int>> expected_st{ { signature,1 },{ signature,2 },{ signature,1 },{ signature,1 },{ signature,4 },{ signature,2 } };
    vector<pair<unsigned int, unsigned int>> actual_st;

    actual_st.push_back( list_st.peek_back() );

    list_st.push_back();
    actual_st.push_back(list_st.peek_back());

    list_st.pop_back();
    actual_st.push_back(list_st.peek_back());

    list_st.pop_back();
    actual_st.push_back(list_st.peek_back());

    list_st.push_back();
    list_st.push_back();
    list_st.push_back();
    actual_st.push_back(list_st.peek_back());

    list_st.pop_back();
    list_st.pop_back();
    actual_st.push_back(list_st.peek_back());

    if (actual_st != expected_st)
    {
        throw logic_error("bad test code -> test_linked_list_single_threaded");
    }
}

//
// Convert the single threaded list (ie not multi-thread safe)
// to a multi-thread safe list
// using the lockfree shared_mutex.
//
class linked_list_multi_threaded: public linked_list_single_threaded
{
public:
    void pop_back()
    {
        unique_lock<lockfree::shared_mutex> ul(m_sm);

        return linked_list_single_threaded::pop_back();
    }

    void push_back()
    {
        unique_lock<lockfree::shared_mutex> ul(m_sm);

        return linked_list_single_threaded::push_back();
    }

    pair<unsigned int, unsigned int> peek_back()
    {
        shared_lock<lockfree::shared_mutex> sl(m_sm);

        return linked_list_single_threaded::peek_back();
    }

private:
    lockfree::shared_mutex m_sm;
};

void testcase_container()
{
    test_linked_list_single_threaded<linked_list_multi_threaded>();
}

//#define PRINT_OUTPUT

void testcase_container_parallelism()
{
    queue<int> qlf;
    queue<int> result;

    const int  = 999;
    static const unsigned int signature = linked_list::parallelism;
    static const unsigned int signature = linked_list::signature;

    {
        vector<future<void>> vf;
        auto random_yield = [parallelism](){
            random_device r;
            unsigned int times = r() % parallelism;
            for(unsigned int c=0; c<times; ++c) this_thread::yield();
        };

        for (int c = 0; (c + 3) <= parallelism; c += 3)
        {
            vf.emplace_back(async(std::launch::async, [&qlf, c, random_yield]() {random_yield(); qlf.push(c + 1); }));
            vf.emplace_back(async(std::launch::async, [&qlf, c, random_yield]() {random_yield(); qlf.push(c + 2); }));
            vf.emplace_back(async(std::launch::async, [&qlf, c, random_yield]() {random_yield(); qlf.push(c + 3); }));
            vf.emplace_back(async(std::launch::async, [&qlf, &result]() {int ip = 0; if (qlf.pop(ip)) result.push(ip); }));
            vf.emplace_back(async(std::launch::async, [&qlf, &result]() {int ip = 0; if (qlf.pop(ip)) result.push(ip); }));
            vf.emplace_back(async(std::launch::async, [&qlf, &result]() {int ip = 0; if (qlf.pop(ip)) result.push(ip); }));
        }

        for (auto & task : vf)
        {
            task.wait();
        }
    }

    int i = 0;
    while (qlf.pop(i))
    {
        result.push(i);
    }

    cout << ' ' << '\n';
    set<int> output;
    while (result.pop(i))
    {
        output.insert(i);
#ifdef PRINT_OUTPUT
        cout << ' ' << i;
#endif
    }
    cout << ' ' << '\n';

    set<int> expected;
    for (int c = 0; (c + 3) <= parallelism; c += 3)
    {
        expected.insert(c + 1);
        expected.insert(c + 2);
        expected.insert(c + 3);
    }

    if (output != expected)
    {
        cout << "\n FAIL";
    }
    else
    {
        cout << "\n success";
    }
    cout << " : test container parallelism.";
}

int main(int argc, char ** argv)
{
    testcase_sanity();
    testcase_container();
    testcase_container_parallelism();

    cout << "\ndone" << flush;
    getchar();
    return 0;
}


