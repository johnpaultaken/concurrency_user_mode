//
// use the following command line to build using gcc
// g++ -std=c++11 -pthread -march=native test_stack.cpp
//

#include "stack.h"

#include <iostream>
#include <future>
#include <vector>

using namespace std;
using namespace lockfree;

template<typename T>
void print(stack<T> & s)
{
    T i{};
    while (s.pop(i))
    {
        cout << i << ' ';
    }
}

int main(int argc, char ** argv)
{
    stack<int> slf;
    stack<int> result;

    {
        vector<future<void>> vf;

        for (int c = 0; c <= 9; c+=3)
        {
            vf.emplace_back(async([&slf, c]() {slf.push(c+1); }));
            vf.emplace_back(async([&slf, c]() {slf.push(c+2); }));
            vf.emplace_back(async([&slf, c]() {slf.push(c+3); }));
            vf.emplace_back(async([&slf, &result]() {int ip = 0; if (slf.pop(ip)) result.push(ip); }));
            vf.emplace_back(async([&slf, &result]() {int ip = 0; if (slf.pop(ip)) result.push(ip); }));
            vf.emplace_back(async([&slf, &result]() {int ip = 0; if (slf.pop(ip)) result.push(ip); }));
        }

        for (auto & task : vf)
        {
            task.wait();
        }
    }

    int i = 0;
    while (slf.pop(i))
    {
        result.push(i);
    }

    // expected output is all numbers from 1 to 12 in any order.
    print(result);

    cout << "\ndone" << flush;
    getchar();
    return 0;
}

