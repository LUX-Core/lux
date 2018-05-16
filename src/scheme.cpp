// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "scheme.h"
#include "reverselock.h"
#include "random.h"
#include <assert.h>
#include <boost/bind.hpp>
#include <utility>

CScheme::CScheme() : nThreadsServicingQueue(0), stopRequested(false), stopWhenEmpty(false)
{
}

CScheme::~CScheme()
{
    assert(nThreadsServicingQueue == 0);
}


#if BOOST_VERSION < 105000
static boost::system_time toPosixTime(const boost::chrono::system_clock::time_point& t)
{
    // Creating the posix_time using from_time_t loses sub-second precision. So rather than exporting the time_point to time_t,
    // start with a posix_time at the epoch (0) and add the milliseconds that have passed since then.
    return boost::posix_time::from_time_t(0) + boost::posix_time::milliseconds(boost::chrono::duration_cast<boost::chrono::milliseconds>(t.time_since_epoch()).count());
}
#endif

bool CScheme::AreThreadsServicingQueue() const {
    boost::unique_lock<boost::mutex> lock(newTaskMutex);
    return nThreadsServicingQueue;
}

void SingleThreadedSchemeClient::MaybeSchemeProcessQueue() {
    {
        LOCK(m_cs_callbacks_pending);
        // Try to avoid scheduling too many copies here, but if we
        // accidentally have two ProcessQueue's scheduled at once its
        // not a big deal.
        if (m_are_callbacks_running) return;
        if (m_callbacks_pending.empty()) return;
    }
    m_pscheme->scheme(std::bind(&SingleThreadedSchemeClient::ProcessQueue, this));
}

void SingleThreadedSchemeClient::ProcessQueue() {
    std::function<void (void)> callback;
    {
        LOCK(m_cs_callbacks_pending);
        if (m_are_callbacks_running) return;
        if (m_callbacks_pending.empty()) return;
        m_are_callbacks_running = true;

        callback = std::move(m_callbacks_pending.front());
        m_callbacks_pending.pop_front();
    }

    // RAII the setting of fCallbacksRunning and calling MaybeSchemeProcessQueue
    // to ensure both happen safely even if callback() throws.
    struct RAIICallbacksRunning {
        SingleThreadedSchemeClient* instance;
        explicit RAIICallbacksRunning(SingleThreadedSchemeClient* _instance) : instance(_instance) {}
        ~RAIICallbacksRunning() {
            {
                LOCK(instance->m_cs_callbacks_pending);
                instance->m_are_callbacks_running = false;
            }
            instance->MaybeSchemeProcessQueue();
        }
    } raiicallbacksrunning(this);

    callback();
}

void SingleThreadedSchemeClient::AddToProcessQueue(std::function<void (void)> func) {
    assert(m_pscheme);

    {
        LOCK(m_cs_callbacks_pending);
        m_callbacks_pending.emplace_back(std::move(func));
    }
    MaybeSchemeProcessQueue();
}

void SingleThreadedSchemeClient::EmptyQueue() {
    assert(!m_pscheme->AreThreadsServicingQueue());
    bool should_continue = true;
    while (should_continue) {
        ProcessQueue();
        LOCK(m_cs_callbacks_pending);
        should_continue = !m_callbacks_pending.empty();
    }
}

size_t SingleThreadedSchemeClient::CallbacksPending() {
    LOCK(m_cs_callbacks_pending);
    return m_callbacks_pending.size();
}

void CScheme::serviceQueue()
{
    boost::unique_lock<boost::mutex> lock(newTaskMutex);
    ++nThreadsServicingQueue;

    // newTaskMutex is locked throughout this loop EXCEPT
    // when the thread is waiting or when the user's function
    // is called.
    while (!shouldStop()) {
        try {
            if (!shouldStop() && taskQueue.empty()) {
                reverse_lock<boost::unique_lock<boost::mutex> > rlock(lock);
                // Use this chance to get a tiny bit more entropy
                //RandAddSeedSleep();
            }
            while (!shouldStop() && taskQueue.empty()) {
                // Wait until there is something to do.
                newTaskScheme.wait(lock);
            }
// Wait until either there is a new task, or until
// the time of the first item on the queue:

// wait_until needs boost 1.50 or later; older versions have timed_wait:
#if BOOST_VERSION < 105000
            while (!shouldStop() && !taskQueue.empty() &&
                   newTaskScheme.timed_wait(lock, toPosixTime(taskQueue.begin()->first))) {
                // Keep waiting until timeout
            }
#else
            // Some boost versions have a conflicting overload of wait_until that returns void.
            // Explicitly use a template here to avoid hitting that overload.
            while (!shouldStop() && !taskQueue.empty()) {
                boost::chrono::system_clock::time_point timeToWaitFor = taskQueue.begin()->first;
                if (newTaskScheme.wait_until<>(lock, timeToWaitFor) == boost::cv_status::timeout)
                    break; // Exit loop after timeout, it means we reached the time of the event
            }
#endif
            // If there are multiple threads, the queue can empty while we're waiting (another
            // thread may service the task we were waiting on).
            if (shouldStop() || taskQueue.empty())
                continue;

            Function f = taskQueue.begin()->second;
            taskQueue.erase(taskQueue.begin());

            {
                // Unlock before calling f, so it can reschedule itself or another task
                // without deadlocking:
                reverse_lock<boost::unique_lock<boost::mutex> > rlock(lock);

                f();
            }
        } catch (...) {
            --nThreadsServicingQueue;
            throw;
        }
    }
    --nThreadsServicingQueue;
    newTaskScheme.notify_one();
}

void CScheme::stop(bool drain)
{
    {
        boost::unique_lock<boost::mutex> lock(newTaskMutex);
        if (drain)
            stopWhenEmpty = true;
        else
            stopRequested = true;
    }
    newTaskScheme.notify_all();
}

void CScheme::scheme(CScheme::Function f, boost::chrono::system_clock::time_point t)
{
    {
        boost::unique_lock<boost::mutex> lock(newTaskMutex);
        taskQueue.insert(std::make_pair(t, f));
    }
    newTaskScheme.notify_one();
}

void CScheme::schemeFromNow(CScheme::Function f, int64_t deltaMilliSeconds)
{
    scheme(f, boost::chrono::system_clock::now() + boost::chrono::milliseconds(deltaMilliSeconds));
}

static void Repeat(CScheme* s, CScheme::Function f, int64_t deltaMilliSeconds)
{
    f();
    s->schemeFromNow(boost::bind(&Repeat, s, f, deltaMilliSeconds), deltaMilliSeconds);
}

void CScheme::schemeEvery(CScheme::Function f, int64_t deltaMilliSeconds)
{
    schemeFromNow(boost::bind(&Repeat, this, f, deltaMilliSeconds), deltaMilliSeconds);
}

size_t CScheme::getQueueInfo(boost::chrono::system_clock::time_point &first,
                                boost::chrono::system_clock::time_point &last) const
{
    boost::unique_lock<boost::mutex> lock(newTaskMutex);
    size_t result = taskQueue.size();
    if (!taskQueue.empty()) {
        first = taskQueue.begin()->first;
        last = taskQueue.rbegin()->first;
    }
    return result;
}