#include "singleton.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <future>

using namespace std;

class C
{
public:
    C() :m_i(0)
    {
        this_thread::sleep_for(chrono::seconds(10));
    }

    C & operator=(int i)
    {
        m_i = i;
        return *this;
    }

    operator int()
    {
        return m_i;
    }
private:
    int m_i;
};

int main(int argc, char ** argv)
{
    auto somecode = []() {
        auto * s = Singleton<C>::get();
        *s = 17;
        cout << *s;
    };

    {
        vector<future<void>> vt;
        for (size_t i = 0; i < 30; ++i)
        {
            vt.push_back(async(somecode));
        }
        for (auto & task : vt)
        {
            task.wait();
        }
    }

    cout << "\ndone";
    getchar();
}
