// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCHEME_H
#define BITCOIN_SCHEME_H

//
// NOTE:
// boost::thread / boost::chrono should be ported to std::thread / std::chrono
// when we support C++11.
//
#include <boost/function.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/thread.hpp>
#include <map>

#include <sync.h>

//
// Simple class for background tasks that should be run
// periodically or once "after a while"
//
// Usage:
//
// CScheme* s = new CScheme();
// s->schemeFromNow(doSomething, 11); // Assuming a: void doSomething() { }
// s->schemeFromNow(boost::bind(Class::func, this, argument), 3);
// boost::thread* t = new boost::thread(boost::bind(CScheme::serviceQueue, s));
//
// ... then at program shutdown, clean up the thread running serviceQueue:
// t->interrupt();
// t->join();
// delete t;
// delete s; // Must be done after thread is interrupted/joined.
//

class CScheme
{
public:
    CScheme();
    ~CScheme();

    typedef boost::function<void(void)> Function;

    // Call func at/after time t
    void scheme(Function f, boost::chrono::system_clock::time_point t=boost::chrono::system_clock::now());

    // Convenience method: call f once deltaSeconds from now
    void schemeFromNow(Function f, int64_t deltaMilliSeconds);

    // Another convenience method: call f approximately
    // every deltaSeconds forever, starting deltaSeconds from now.
    // To be more precise: every time f is finished, it
    // is rescheduled to run deltaSeconds later. If you
    // need more accurate scheduling, don't use this method.
    void schemeEvery(Function f, int64_t deltaMilliSeconds);

    // To keep things as simple as possible, there is no unschedule.

    // Services the queue 'forever'. Should be run in a thread,
    // and interrupted using boost::interrupt_thread
    void serviceQueue();

    // Tell any threads running serviceQueue to stop as soon as they're
    // done servicing whatever task they're currently servicing (drain=false)
    // or when there is no work left to be done (drain=true)
    void stop(bool drain=false);

    // Returns number of tasks waiting to be serviced,
    // and first and last task times
    size_t getQueueInfo(boost::chrono::system_clock::time_point &first,
                        boost::chrono::system_clock::time_point &last) const;

    // Returns true if there are threads actively running in serviceQueue()
    bool AreThreadsServicingQueue() const;

private:
    std::multimap<boost::chrono::system_clock::time_point, Function> taskQueue;
    boost::condition_variable newTaskScheme;
    mutable boost::mutex newTaskMutex;
    int nThreadsServicingQueue;
    bool stopRequested;
    bool stopWhenEmpty;
    bool shouldStop() const { return stopRequested || (stopWhenEmpty && taskQueue.empty()); }
};

/**
 * Class used by CScheme clients which may scheme multiple jobs
 * which are required to be run serially. Does not require such jobs
 * to be executed on the same thread, but no two jobs will be executed
 * at the same time.
 */
class SingleThreadedSchemeClient {
private:
    CScheme *m_pscheme;

    CCriticalSection m_cs_callbacks_pending;
    std::list<std::function<void (void)>> m_callbacks_pending;
    bool m_are_callbacks_running = false;

    void MaybeSchemeProcessQueue();
    void ProcessQueue();

public:
    explicit SingleThreadedSchemeClient(CScheme *pschemeIn) : m_pscheme(pschemeIn) {}
    void AddToProcessQueue(std::function<void (void)> func);

    // Processes all remaining queue members on the calling thread, blocking until queue is empty
    // Must be called after the CScheme has no remaining processing threads!
    void EmptyQueue();

    size_t CallbacksPending();
};

#endif
