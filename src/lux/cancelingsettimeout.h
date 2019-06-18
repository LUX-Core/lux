#ifndef LUX_CANCELINGSETTIMEOUT_H
#define LUX_CANCELINGSETTIMEOUT_H

#include <chrono>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace ch = std::chrono;

class CancelingSetTimeout
{
protected:
    std::thread thread;
    std::condition_variable cv;
    std::mutex cv_m;
    ch::time_point<ch::system_clock> start_time;
    bool exit;

    void Start(ch::milliseconds delay, std::function<void()> &cancel_f, std::function<void()> &timeout_f) {
        std::unique_lock<std::mutex> lk(cv_m);
        if(cv.wait_for(lk, delay, [this, delay]() -> bool {
            auto now = ch::system_clock::now();
            return exit;
        })) {
            if (cancel_f) {
                cancel_f();
            }
        } else {
            if (timeout_f) {
                timeout_f();
            }
        }
    }

public:
    CancelingSetTimeout(std::chrono::milliseconds delay, std::function<void()> f, std::function<void()> timeout_f) :
            exit(false),
            start_time(std::chrono::system_clock::now()),
            thread(std::bind(&CancelingSetTimeout::Start, this, delay, f, timeout_f)) {}

    void Cancel() {
        {
            std::lock_guard<std::mutex> lk(cv_m);
            exit = true;
        }
        cv.notify_one();
    }

    ~CancelingSetTimeout() {
        thread.join();
    }
};

#endif //LUX_CANCELINGSETTIMEOUT_H
