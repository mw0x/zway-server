/*
 * thread.h
 *
 *  Created on: 25.11.2012
 *      Author: marc
 */

#ifndef THREAD_H_
#define THREAD_H_

#include <boost/thread.hpp>

// ============================================================ //
// ThreadSafe
// ============================================================ //

template <class T>
class ThreadSafe
{
    public:
        ThreadSafe()
        {
        }

        ThreadSafe(T t)
        {
            m_t = t;
        }

        ThreadSafe& operator=(T t)
        {
            m_t = t;

            return *this;
        }

        operator T&() const
        {
            return (T&)m_t;
        }

        operator T&()
        {
            return m_t;
        }

        T& operator*()
        {
            return m_t;
        }

        T* operator->()
        {
            return &m_t;
        }

        operator boost::mutex&()
        {
            return m_mutex;
        }

    protected:

        T m_t;

        boost::mutex m_mutex;
};

// ============================================================ //

#endif /* THREAD_H_ */
