//
// use the following command line to build using gcc
// g++ -std=c++14 -pthread test_shared_mutex.cpp
//
// Note: C++14 is needed for compiling test only, not shared_mutex.h
// It is because we want to test our shared_mutex is compliant with the wrapper 'shared_lock' from C++14
//
// This test shows how to easily convert your single thread use only container
// into a multi-thread safe container
// using the lockfree shared_mutex from this repo.
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
#include <stdexcept>

using std::cout;
using std::pair;
using std::make_pair;
using std::vector;
using std::logic_error;
using std::flush;
using std::future;
using std::random_device;

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
    cout << " : sanity test - single threaded shared and exclusive locking." << std::flush;
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
                checkNodeForMemoryCorruption(newlast);
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
                checkNodeForMemoryCorruption(currentlast);
            }
            currentlast->next = newnode;
        }
    }

    pair<unsigned int, unsigned int> peek_back()
    {
        auto currentlast = &head;
        unsigned int numnodes = 0;
        while (currentlast->next)
        {
            currentlast = currentlast->next;
            checkNodeForMemoryCorruption(currentlast);
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

    void checkNodeForMemoryCorruption(node * pNode)
    {
        if (pNode == (&badnode))
        {
            throw std::logic_error("Likely thread race data corruption. Bad node found!");
        }

        if (pNode->element == badcode)
        {
            throw std::logic_error("Likely thread race data corruption. Bad code found!");
        }
    }

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

    vector<pair<unsigned int, unsigned int>> expected_st{ { signature,0 },{ signature,1 },{ signature,0 },{ signature,0 },{ signature,3 },{ signature,1 } };
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
    try
    {
        test_linked_list_single_threaded<linked_list_multi_threaded>();
        cout << "\n success";
    }
    catch (std::exception & e)
    {
        cout << "\n FAIL";
    }

    cout << " : single threaded test - multi thread safe container." << std::flush;
}

//#define PRINT_OUTPUT

void testcase_container_parallelism()
{
    linked_list_multi_threaded mtl;

    static const unsigned int maxyield = 6;

    bool failed = false;

    {
        vector<future<void>> vf;

        auto random_yield = [](){
            random_device r;
            unsigned int times = r() % maxyield;
            for(unsigned int c=0; c<times; ++c) std::this_thread::yield();
        };

        auto push = [&mtl, random_yield]() {
            random_yield();
            auto pr = mtl.peek_back();
            if (pr.first != mtl.signature) throw std::logic_error("umatched signature");
            if (pr.second > mtl.parallelism) throw std::logic_error("unexpected number of nodes");
            mtl.push_back();
        };

        auto pop = [&mtl, random_yield]() {
            random_yield();
            auto pr = mtl.peek_back();
            if (pr.first != mtl.signature) throw std::logic_error("umatched signature");
            if (pr.second > mtl.parallelism) throw std::logic_error("unexpected number of nodes");
            mtl.pop_back();
        };

        auto peek = [&mtl, random_yield]() {
            random_yield();
            auto pr = mtl.peek_back();
            if (pr.first != mtl.signature) throw std::logic_error("umatched signature");
            if (pr.second > mtl.parallelism) throw std::logic_error("unexpected number of nodes");
        };

        for (int c = 0; c < 2*mtl.parallelism; ++c)
        {
            // Much larger reads compared to writes.
            vf.emplace_back(async(std::launch::async, peek));
            vf.emplace_back(async(std::launch::async, peek));
            vf.emplace_back(async(std::launch::async, peek));
            vf.emplace_back(async(std::launch::async, push));
            vf.emplace_back(async(std::launch::async, push));
            vf.emplace_back(async(std::launch::async, pop));
        }

        try
        {
            for (auto & task : vf)
            {
                task.get();
            }
        }
        catch (std::exception & e)
        {
            failed = true;
#ifdef PRINT_OUTPUT
            std::cerr << "\n" << e.what();
#endif
        }
    }

    if (failed)
    {
        cout << "\n FAIL";
    }
    else
    {
        cout << "\n success";
    }
    cout << " : parallelism test - multi thread safe container." << std::flush;
}

int main(int argc, char ** argv)
{
    testcase_sanity();
    testcase_container();
    testcase_container_parallelism();

    cout << "\ndone\n" << flush;
    //getchar();
    return 0;
}


