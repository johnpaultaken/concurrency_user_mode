//
// use the following command line to build using gcc
// g++ -std=c++11 -pthread -march=native test_queue.cpp
//

#include "shared_mutex.h"

#include <iostream>
#include <future>
#include <vector>
#include <set>
#include <random>

using namespace std;
using namespace lockfree;

void testcase_sanity()
{
    shared_mutex sm;

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

void testcase_parallelism()
{
    const unsigned int parallelism = 999;
    const unsigned int signature = 0x12345678;
    const unsigned int badcode = 0xbadc0de;

    class linked_list_single_threaded
    {
    public:
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

    linked_list_single_threaded list_st;

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
        throw logic_error("bad test code -> testcase_parallelism::linked_list_single_threaded");
    }


    /*
    if (actual_st != expected_st)
    {
        cout << "\n FAIL";
    }
    else
    {
        cout << "\n success";
    }
    cout << " test parallelism: single threaded list test";
    */
}

int main(int argc, char ** argv)
{
    testcase_sanity();
    testcase_parallelism();

    cout << "\ndone" << flush;
    getchar();
    return 0;
}


#ifdef JUNK

//#define PRINT_OUTPUT


void testcase_parallelism()
{
    queue<int> qlf;
    queue<int> result;

    const int parallelism = 999;

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
    cout << " test parallelism: push pop in parallel";
}

void testcase_queueSemantic_pushpop()
{
    vector<int> input{ 1,2,3,4,5 };
    vector<int> output;

    vector<int> expected{ 1,2,3,4,5 };

    queue<int> q;
    for (auto v : input)
    {
        q.push(v);
    }

    int i = 0;
    while (q.pop(i))
    {
        output.push_back(i);
    }

    if (output != expected)
    {
        cout << "\n FAIL";
    }
    else
    {
        cout << "\n success";
    }
    cout << " test queueSemantic: push pop sequence";

#ifdef PRINT_OUTPUT
    for (auto v : output)
    {
        cout << ' ' << v;
    }
#endif
}

void testcase_queueSemantic_partialpoppush()
{
    vector<int> input1{ 1,2,3,4,5 };
    vector<int> input2{ 6,7,8 };
    vector<int> output;

    vector<int> expected{ 4,5,6,7,8 };

    queue<int> q;
    for (auto v : input1)
    {
        q.push(v);
    }
    int i = 0;
    for (int c = 0; c < 3; ++c)
    {
        q.pop(i);
    }
    for (auto v : input2)
    {
        q.push(v);
    }

    while (q.pop(i))
    {
        output.push_back(i);
    }

    if (output != expected)
    {
        cout << "\n FAIL";
    }
    else
    {
        cout << "\n success";
    }
    cout << " test queueSemantic: partial pop push sequence";

#ifdef PRINT_OUTPUT
    for (auto v : output)
    {
        cout << ' ' << v;
    }
#endif
}

int main(int argc, char ** argv)
{
    testcase_queueSemantic_pushpop();
    testcase_queueSemantic_partialpoppush();

    testcase_parallelism();

    cout << "\ndone" << flush;
    getchar();
    return 0;
}

#endif //JUNK
