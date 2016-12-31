#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <future>

//-----------------------------------------------------------
#include <atomic>
#include <mutex>
using namespace std;

/*
The first m_pObj.load() in get() need memory_order_acquire to synchronize with 
    release semantic on the atomic m_pObj.store().
The second m_pObj.load() can be memory_order_relaxed.
    This is because if the object was allocated by another thread it has already done
    mutex unlock before this thread locked mutex. The matched release-acquire semantic
    for the mutex would take care of the visibility of object construction and atomic.
The m_pObj.store() needs memory_order_release to syncronize with the first m_pObj.load().
    Note that release semantic of mutex unlock cannot stand in for this because release-acquire
    semantic must match wrt the same atomic.

The following link says this implementation is correct and is the best.
    http://preshing.com/20130930/double-checked-locking-is-fixed-in-cpp11/
TODO:
 1.
 A faster implementation seems possible by  not combining the fence and atomic.load()
 even though the above link is not doing that.
*/

template <class T>
class Singleton
{
public:
    static T * get()
    {
        auto pObj = m_pObj.load(memory_order_acquire);
        if (!pObj)
        {
            lock_guard<mutex> lock(m_mtx);
            pObj = m_pObj.load(memory_order_relaxed);
            if (!pObj)
            {
                pObj = new T();
                m_pObj.store(pObj, memory_order_release);
            }
        }
        return pObj;
    }

private:
    static atomic<T *> m_pObj;
    static mutex m_mtx;
};

template <class T>
mutex Singleton<T>::m_mtx;

template <class T>
atomic<T *> Singleton<T>::m_pObj(nullptr);
//-----------------------------------------------------------

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
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
        vt.push_back(async(somecode));
    }

    cout << "\ndone";
    getchar();
}
