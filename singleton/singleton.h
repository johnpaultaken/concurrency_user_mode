#pragma once

#include <atomic>
#include <mutex>

using std::memory_order_relaxed;
using std::memory_order_acquire;
using std::memory_order_release;

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
            std::lock_guard<std::mutex> lock(m_mtx);
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
    static std::atomic<T *> m_pObj;
    static std::mutex m_mtx;
};

template <class T>
std::mutex Singleton<T>::m_mtx;

template <class T>
std::atomic<T *> Singleton<T>::m_pObj(nullptr);
