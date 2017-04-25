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

// To enable output of more info on failure.
#define PRINT_TRACE

// To enable stress testing until failure.
//#define STRESS_TEST

// To enable testing of the test code itself,
// especially the parallelism test function testcase_container_parallelism().
// Enabling this line requires compile option "g++ -std=c++17".
//#define TEST_TESTCODE


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
#include <chrono>
#include <iomanip>

using std::cout;
using std::pair;
using std::make_pair;
using std::vector;
using std::logic_error;
using std::flush;
using std::future;
using std::random_device;
namespace chrono = std::chrono;


bool testcase_sanity()
{
    bool bResult = false;
    try
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
        bResult = true;
    }
    catch (std::exception & e)
    {
#ifdef PRINT_TRACE
        std::cerr << "\n" << e.what();
#endif
        cout << "\n FAIL";
    }

    cout << " : sanity test - single threaded shared and exclusive locking." << std::flush;
    return bResult;
}


class linked_list_single_threaded
{
public:
    static const unsigned int capacity = 999;
    static const unsigned int signature_allocated = 0x12345678;
    static const unsigned int signature_freed = 0xbadc0de;

    linked_list_single_threaded()
    {
        head.element = signature_allocated;
        head.next = nullptr;

        node_deadend.element = signature_freed;
        node_deadend.next = &node_deadend;    // to cause infinite looping.

        for (unsigned int i = 0; i < capacity; ++i)
        {
            freelist[i].allocated = false;
            freelist[i].element = signature_freed;
            freelist[i].next = &node_deadend;
        }
    }

    void pop_back()
    {
        if (head.next)
        {
            auto newlast = &head;
            unsigned int numnodes = 0;
            while (newlast->next->next)
            {
                newlast = newlast->next;
                numnodes++;
                verifyAllocatedNode(newlast, numnodes);
            }
            newlast->next->allocated = false;
            newlast->next->element = signature_freed;
            newlast->next->next = &node_deadend;

            newlast->next = nullptr;
        }
    }

    void push_back()
    {
        node * newnode = nullptr;
        for (unsigned int i = 0; i < capacity; ++i)
        {
            if (! freelist[i].allocated)
            {
                newnode = &freelist[i];
                freelist[i].allocated = true;
                freelist[i].element = signature_allocated;
                freelist[i].next = nullptr;

                break;
            }
        }

        if (newnode)
        {
            auto currentlast = &head;
            unsigned int numnodes = 0;
            while (currentlast->next)
            {
                currentlast = currentlast->next;
                numnodes++;
                verifyAllocatedNode(currentlast, numnodes);
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
            numnodes++;
            verifyAllocatedNode(currentlast, numnodes);
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

    void verifyAllocatedNode(node * pNode, unsigned int seqNum)
    {
        if (seqNum > capacity)
        {
            throw std::logic_error("Capacity exceeded. Likely thread race data corruption.");
        }

        if (pNode->element != signature_allocated)
        {
            throw std::logic_error("Bad signature in allocated node. Likely thread race data corruption.");
        }

        if (pNode == (&node_deadend))
        {
            throw std::logic_error("Reached dead end. Likely thread race data corruption.");
        }
    }

    // nodes pre allocated to avoid locks in allocator.
    node freelist[capacity];

    node node_deadend;

    node head;
};

template<typename linked_list>
void test_linked_list_single_threaded()
{
    linked_list list_st;

    static const unsigned int signature_allocated = linked_list::signature_allocated;

    vector<pair<unsigned int, unsigned int>> expected_st{ { signature_allocated,0 },{ signature_allocated,1 },{ signature_allocated,0 },{ signature_allocated,0 },{ signature_allocated,3 },{ signature_allocated,1 } };
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
template<class shared_mutex>
class linked_list_multi_threaded: public linked_list_single_threaded
{
public:
    void pop_back()
    {
        unique_lock<shared_mutex> ul(m_sm);

        return linked_list_single_threaded::pop_back();
    }

    void push_back()
    {
        unique_lock<shared_mutex> ul(m_sm);

        return linked_list_single_threaded::push_back();
    }

    pair<unsigned int, unsigned int> peek_back()
    {
        shared_lock<shared_mutex> sl(m_sm);

        return linked_list_single_threaded::peek_back();
    }

private:
    shared_mutex m_sm;
};

#ifdef TEST_TESTCODE
using linked_list_mt = linked_list_multi_threaded<std::shared_mutex>;
#else
using linked_list_mt = linked_list_multi_threaded<lockfree::shared_mutex>;
#endif

bool testcase_container()
{
    bool bResult = false;
    try
    {
        test_linked_list_single_threaded<linked_list_mt>();
        cout << "\n success";
        bResult = true;
    }
    catch (std::exception & e)
    {
#ifdef PRINT_TRACE
        std::cerr << "\n" << e.what();
#endif
        cout << "\n FAIL";
    }

    cout << " : single threaded test - multi thread safe container." << std::flush;
    return bResult;
}

bool testcase_container_parallelism(bool suppress_output = false)
{
    linked_list_mt mtl;

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
            mtl.push_back();
        };

        auto pop = [&mtl, random_yield]() {
            random_yield();
            mtl.pop_back();
        };

        auto peek = [&mtl, random_yield]() {
            random_yield();
            auto pr = mtl.peek_back();
            if (pr.first != mtl.signature_allocated) throw std::logic_error("umatched signature");
            if (pr.second > mtl.capacity) throw std::logic_error("unexpected number of nodes");
        };

        for (int c = 0; c < 2*mtl.capacity; ++c)
        {
            // Much larger reads compared to writes.
            vf.emplace_back(async(std::launch::async, peek));
            vf.emplace_back(async(std::launch::async, push));
            vf.emplace_back(async(std::launch::async, peek));
            vf.emplace_back(async(std::launch::async, push));
            vf.emplace_back(async(std::launch::async, peek));
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
#ifdef PRINT_TRACE
            std::cerr << "\n" << e.what() << std::flush;
#endif
        }
    }

    if (!suppress_output)
    {
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

    return (!failed);
}

#define RUN_TEST(testcase) { if (!testcase()) return 1;}
int main(int argc, char ** argv)
{
    RUN_TEST(testcase_sanity);
    RUN_TEST(testcase_container);
    RUN_TEST(testcase_container_parallelism);

#ifdef STRESS_TEST
    auto start = chrono::system_clock::now();
    unsigned run_count = 0;
    cout << "\n";
    while (testcase_container_parallelism(true))
    {
        run_count++;
        cout << " run_count: " << run_count << "\r" << std::flush;
    }
    auto stop = chrono::system_clock::now();

    auto run_nano = stop - start;
    auto run_min = run_nano.count() / 1000000000 / 60;
    cout << "\n Failed after " << run_min / 60 << ":"
        << std::setw(2) << std::setfill ('0')
        << run_min % 60
        << std::setw(0) << std::setfill (' ')
        << " hrs"
    ;
#endif // STRESS_TEST

    cout << "\ndone\n" << flush;
    //getchar();
    return 0;
}


