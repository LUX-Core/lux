// Copyright (c) 2017-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "metrics.h"

#include "chainparams.h"
#include "util.h"
#include "utiltime.h"

#include <boost/thread.hpp>

AtomicCounter transactionsValidated;
AtomicCounter PhiRuns;
AtomicCounter minedBlocks;
void ThreadShowMetricsScreen()
{
    RenameThread(" >>> Luxcore - FIRST OF ITS KIND <<<");
    std::cout << "\e[2J";
    std::cout << METRICS_ART << std::endl;
    std::cout << std::endl;
    std::cout << OFFSET << " >>> Thank for running luxcore node <<< " << std::endl;
    std::cout << OFFSET << "https://luxcore.io" << std::endl;
    std::cout << OFFSET << "https://github.com/LUX-Core/lux" << std::endl;
    std::cout << std::endl;
    bool staking = GetBoolArg("-staking", false);
    if (staking) {
        int nThreads = GetArg("-staking", 1);
        if (nThreads < 0) {
            if (Params().DefaultStakeThreads())
                nThreads = Params().DefaultStakeThreads();
            else
                nThreads = boost::thread::hardware_concurrency();
        }
        std::cout << OFFSET << "Running " << nThreads << " staking threads !!!" << std::endl;
    } else {
        std::cout << OFFSET << "You are currently not staking." << std::endl;
        std::cout << OFFSET << "To enable staking, add 'gen=1' to your lux.conf and restart." << std::endl;
    }
    std::cout << std::endl;
    int64_t nStart = GetTime();
    while (true) {
        int lines = 4;
        std::cout << "\e[J";
        int64_t uptime = GetTime() - nStart;
        int days = uptime / (24 * 60 * 60);
        int hours = (uptime - (days * 24 * 60 * 60)) / (60 * 60);
        int minutes = (uptime - (((days * 24) + hours) * 60 * 60)) / 60;
        int seconds = uptime - (((((days * 24) + hours) * 60) + minutes) * 60);
        std::cout << OFFSET << "Since starting this node ";
        if (days > 0) {
            std::cout << days << " days, ";
        }
        if (hours > 0) {
            std::cout << hours << " hours, ";
        }
        if (minutes > 0) {
            std::cout << minutes << " minutes, ";
        }
        std::cout << seconds << " seconds ago:" << std::endl;
        std::cout << OFFSET << "- You have validated " << transactionsValidated.get() << " transactions." << std::endl;
        if (staking) {
            std::cout << OFFSET << "- You have completed " << PhiRuns.get() << ">>> staking <<<" << std::endl;
            lines++;

            int mined = minedBlocks.get();
            if (mined > 0) {
                std::cout << OFFSET << ">>> You have staked <<< " << mined << " blocks!" << std::endl;
                lines++;
            }
        }
        std::cout << std::endl;
        std::cout << "[Hit Ctrl+C to exit] [Set 'showmetrics=0' in lux.conf to hide]" << std::endl;;
        boost::this_thread::interruption_point();
        MilliSleep(1000);
        std::cout << "\e[" << lines << "A";
    }
}
