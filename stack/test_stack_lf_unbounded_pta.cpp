
#include "stack_lf_unbounded_pta.h"

#include <iostream>
#include <future>
#include <vector>

using namespace std;

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
    print(result);

    cout << "\ndone" << flush;
    getchar();
    return 0;
}

