// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sync.h"

#include "util.h"
#include "utilstrencodings.h"

#include <stdio.h>

#include <boost/thread.hpp>

#ifdef DEBUG_LOCKCONTENTION
#if !defined(HAVE_THREAD_LOCAL)
static_assert(false, "thread_local is un-supported");
#endif
void PrintLockContention(const char* pszName, const char* pszFile, int nLine)
{
    LogPrintf("LOCKCONTENTION: %s\n", pszName);
    LogPrintf("Locker: %s:%d\n", pszFile, nLine);
}
#endif /* DEBUG_LOCKCONTENTION */

#ifdef DEBUG_LOCKORDER
//
// Early deadlock detection.
// Problem being solved:
//    Thread 1 locks  A, then B, then C
//    Thread 2 locks  D, then C, then A
//     --> may result in deadlock between the two threads, depending on when they run.
// Solution implemented here:
// Keep track of pairs of locks: (A before B), (A before C), etc.
// Complain if any thread tries to lock in a different order.
//

struct CLockLocation {
    CLockLocation(const char* pszName, const char* pszFile, int nLine)
    {
        mutexName = pszName;
        sourceFile = pszFile;
        sourceLine = nLine;
    }

    std::string ToString() const
    {
        return mutexName + "  " + sourceFile + ":" + itostr(sourceLine);
    }

    std::string MutexName() const { return mutexName; }

private:
    std::string mutexName;
    std::string sourceFile;
    int sourceLine;
};

typedef std::vector<std::pair<void*, CLockLocation> > LockStack;

static boost::mutex dd_mutex;
static std::map<std::pair<void*, void*>, LockStack> lockorders;
static boost::thread_specific_ptr<LockStack> lockstack;


static void potential_deadlock_detected(const std::pair<void*, void*>& mismatch, const LockStack& s1, const LockStack& s2)
{
    LogPrintf("POTENTIAL DEADLOCK DETECTED\n");
    LogPrintf("Previous lock order was:\n");
    for (const PAIRTYPE(void*, CLockLocation) & i : s2) {
        if (i.first == mismatch.first)
            LogPrintf(" (1)");
        if (i.first == mismatch.second)
            LogPrintf(" (2)");
        LogPrintf(" %s\n", i.second.ToString());
    }
    LogPrintf("Current lock order is:\n");
    for (const PAIRTYPE(void*, CLockLocation) & i : s1) {
        if (i.first == mismatch.first)
            LogPrintf(" (1)");
        if (i.first == mismatch.second)
            LogPrintf(" (2)");
        LogPrintf(" %s\n", i.second.ToString());
    }
}

static void push_lock(void* c, const CLockLocation& locklocation, bool fTry)
{
    if (lockstack.get() == NULL)
        lockstack.reset(new LockStack);

    LogPrint("lock", "Locking: %s\n", locklocation.ToString());
    dd_mutex.lock();

    (*lockstack).push_back(std::make_pair(c, locklocation));

    if (!fTry) {
        for (const PAIRTYPE(void*, CLockLocation) & i : (*lockstack)) {
            if (i.first == c)
                break;

            std::pair<void*, void*> p1 = std::make_pair(i.first, c);
            if (lockorders.count(p1))
                continue;
            lockorders[p1] = (*lockstack);

            std::pair<void*, void*> p2 = std::make_pair(c, i.first);
            if (lockorders.count(p2)) {
                potential_deadlock_detected(p1, lockorders[p2], lockorders[p1]);
                break;
            }
        }
    }
    dd_mutex.unlock();
}

static void pop_lock()
{
    if (fDebug) {
        const CLockLocation& locklocation = (*lockstack).rbegin()->second;
        LogPrint("lock", "Unlocked: %s\n", locklocation.ToString());
    }
    dd_mutex.lock();
    (*lockstack).pop_back();
    dd_mutex.unlock();
}

void EnterCritical(const char* pszName, const char* pszFile, int nLine, void* cs, bool fTry)
{
    push_lock(cs, CLockLocation(pszName, pszFile, nLine), fTry);
}

void LeaveCritical()
{
    pop_lock();
}

std::string LocksHeld()
{
    std::string result;
    for (const PAIRTYPE(void*, CLockLocation) & i : *lockstack)
        result += i.second.ToString() + std::string("\n");
    return result;
}

void AssertLockHeldInternal(const char* pszName, const char* pszFile, int nLine, void* cs)
{
    for (const PAIRTYPE(void*, CLockLocation) & i : *lockstack)
        if (i.first == cs)
            return;
    fprintf(stderr, "Assertion failed: lock %s not held in %s:%i; locks held:\n%s", pszName, pszFile, nLine, LocksHeld().c_str());
    abort();
}

#endif /* DEBUG_LOCKORDER */
