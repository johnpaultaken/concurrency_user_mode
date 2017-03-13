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

int main(int argc, char ** argv)
{
    shared_mutex sm;

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
