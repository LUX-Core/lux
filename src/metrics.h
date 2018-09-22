// Copyright (c) 2017-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <atomic>
#include <string>

struct AtomicCounter {
    std::atomic<int> value;
    AtomicCounter() : value {0} { }
    void increment(){ ++value; }
    void decrement(){ --value; }
    int get(){ return value.load(); }
};
extern AtomicCounter transactionsValidated;
extern AtomicCounter PhiRuns;
extern AtomicCounter minedBlocks;
void ThreadShowMetricsScreen();
const std::string METRICS_ART =
        "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM\n"
        "MMMMMMMMMMMMMMMMMMMMMMMMMM>>!MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM!>>MMMMMMMMMMMMMMMMMMMMMMMMM\n"
        "MMMMMMMMMMMMMMMMMMMMMM>>>>>>>>>>>MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM>>>>>>>>>MMMMMMMMMMMMMMMMMMMMMM\n"
        "MMMMMMMMMMMMMMMMMMM!>>>>>>>>>>>>>>>!MMMMMMMMMMMMMMMMMMMMMMMMMMMMM!>>>>>>>>>>>>>>>MMMMMMMMMMMMMMMMMMM\n"
        "MMMMMMMMMMMMMMM>>>>MMMM!>>>>>>>>>>>>>>>>MMMMMMMMMMMMMMMMMMMMMM>>>>>>>>>>>>>>>>MMMM>>>>MMMMMMMMMMMMMM\n"
        "MMMMMMMMMMM;>>>>>>>>>>>MMM;>>>>>>>>>>>>>>>>MMMMMMMMMMMMMMM>>>>>>>>>>>>>>>>MMMM>>>>>>>>>>>>MMMMMMMMMM\n"
        "MMMMMMMMMM>>>>>>>>>>>>>>>!MMM>>>>>>>>>>>>>>>>>MMMMMMMMMM>>>>>>>>>>>>>>>>MMMM>>>>>>>>>>>>>>>>MMMMMMMM\n"
        "MMMMMMMMMMMM>>>>>>>>>>>>>>>>!MMMM>>>>>>>>>>>>>>>MMMM>>>>>>>>>>>>>>>>>MMM>>>>>>>>>>>>>>>>>MMMMMMMMMMM\n"
        "MMMMMMMM>>>>MMMM>>>>>>>>>>>>>>>>>MMM!>>>>>>>MMMMM>>>>>>>>>>>>>>>>MMMM>>>>>>>>>>>>>>>>MMMM!>>>MMMMMMM\n"
        "MMMMMMMM>>>>>>>>MMM>>>>>>>>>>>>>>>>!MMMM>MMM>>>>>>>>>>>>>>>>>MMMM>>>>>>>>>>>>>>>>>MMM>>>>>>>>MMMMMMM\n"
        "MMMMMMMM>>>>>>>>>MMMMMM!>>>>>>>>>>>>>>MMM>>>>>>>>>>>>>>>>>MMMM>>>>>>>>>>>>>>>>MMMMMMM>>>>>>>>MMMMMMM\n"
        "MMMMMMMM>>>>>>>>>MM>>>!MMMM>>>>>>MMMMM>>>>>>>>>>>>>>>!MMMM>>>>>>>>>>>>>>>>MMMM>>>>>MM>>>>>>>>MMMMMMM\n"
        "MMMMMMMM>>>>>>>>>MM>>>>>>>;MMMMMMM>>>>>>>>>>>>>>>>>MMM>>>>>>>>>>>>>>>>!MMM!>>>>>>>>MM>>>>>>>!MMMMMMM\n"
        "MMMMMMMM!>>>>>>>>MM>>>>>>>MMMM;>>>>>>>>>>>>>>>>MMMM>>>>>>>>>>>>>>>>MMMMMMMM>>>>>>>>MM>>>>>>>!MMMMMMM\n"
        "MMMMMMMM!>>>>>>>>MM>>>>MMM!>>>>>>>>>>>>>>>>MMMM>>>>>>>>>>>>>>>>>MMM>>>>>>>>MMM!>>>>MM>>>>>>>!MMMMMMM\n"
        "MMMMMMMM!>>>>>>>>MMMMMM>>>>>>>>>>>>>>>>>MMM>>>>>>>>>>>>>>>>>MMM!!>>>>>>>>>>>>>MMMMMMM>>>>>>>!MMMMMMM\n"
        "MMMMMMMM!>>>>>>>!MMMM>>>>>>>>>>>>>>>>>MMM>>>>>>>>>>>>>>>>!MMMMMM>>>>>>>>>>>>>>>>>MMMM>>>>>>>!MMMMMMM\n"
        "MMMMMMMM>>>>>MMMM>>>>>>>>>>>>>>>>MMMMM>>>>>>>>>>>>>>>>MMMM>>>>>>MMM>>>>>>>>>>>>>>>>>MMMM>>>>>MMMMMMM\n"
        "MMMMMMMM>>MMM>>>>>>>>>>>>>>>>>MMMM>>>>>>>>>>>>>>>>>MMM>>>>>>>>>>>>>MMMMM>>>>>>>>>>>>>>>>MMMM!MMMMMMM\n"
        "MMMMMMMMMM>>>>>>>>>>>>>>>>MMMM!>>>>>>>>>>>>>>>>MMMMMMM;>>>>>>>>>>>>>>>>MMMM>>>>>>>>>>>>>>>>>MMMMMMMM\n"
        "MMMMMMMM>>>>>>>>>>>>>>>MMM>>>>>>>>>>>>>>>>>MMMMMMMMMMMMMMM>>>>>>>>>>>>>>>>!MMM>>>>>>>>>>>>>>>MMMMMMM\n"
        "MMMMMMMM>>>>>>>>>>>MMMM>>>>>>>>>>>>>>>>>MMMMMMMMMMMMMMMMMMMMMM>>>>>>>>>>>>>>>>MMMM>>>>>>>>>>>MMMMMMM\n"
        "MMMMMMMM>>>>>>>>>MM!>>>>>>>>>>>>>>>>MMMMMMMMMMMMMMMMMMMMMMMMMMMMM!>>>>>>>>>>>>>>>!MMM>>>>>>>>MMMMMMM\n"
        "MMMMMMMM>>>>>>>>>MM>>>>>>>>>>>>>!MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM>>>>>>>>>>>>>>MM>>>>>>>>MMMMMMM\n"
        "MMMMMMMM>>>>>>>>>MM>>>>>>>>>MMMMM>>>!MMMMMMMMMMMMMMMMMMMMMMMMMMMM>>>!MMM>>>>>>>>>>>MM>>>>>>>>MMMMMMM\n"
        "MMMMMMMM>>>>>>>>>MM>>>>>>>>MM>>>>>>>>>>>MMMMMMMMMMMMMMMMMMMMM!>>>>>>>>>>MM>>>>>>>>>MM>>>>>>>>MMMMMMM\n"
        "MMMMMMMM!>>>>>>>>MM>>>>>>>>M;>>>>>>>>>>>>>>>MMMMMMMMMMMMMM>>>>>>>>>>>>>>;M>>>>>>>>>MM>>>>>>>!MMMMMMM\n"
        "MMMMMMMM!>>>>>>>>MM>>>>>>>>MM!>>>>>>>>>>>>>>>>>MMMMMMMM!>>>>>>>>>>>>>>>>MM>>>>>>>>>MM>>>>>>>!MMMMMMM\n"
        "MMMMMMMM!>>>>>>>>MM>>>>>>>>M!MMMM>>>>>>>>>>>>>>>MMM>>>>>>>>>>>>>>>>>MMMM!M>>>>>>>>>MM>>>>>>>!MMMMMMM\n"
        "MMMMMMMM!>>>>>>>>MM>>>>>>>>MM>>>>MMMM>>>>>>>MMMM>>>>>>>>>>>>>>>>>MMM!>>>MM>>>>>>>>>MM>>>>>>>!MMMMMMM\n"
        "MMMMMMMM>>>>>>>>>MM>>>>>>>>MM>>>>>>>>MMMMMMM>>>>>>>>>>>>>>>>>MMMM>>>>>>>MM>>>>>>>>>MM>>>>>>>>MMMMMMM\n"
        "MMMMMMMM>>>>>>>>>MM>>>>>>>>MM>>>>>>>>MMM!>>>>>>>>>>>>>>>>;MMM>>>>>>>>>>>MM>>>>>>>>>MM>>>>>>>>MMMMMMM\n"
        "MMMMMMMM>>>>>>>>>MM>>>>>>>>MM>>>>MMMM>>>>>>>>>>>>>>>>MMMMM>>>>>>>>>>>>>>MM>>>>>>>>>MM>>>>>>>>MMMMMMM\n"
        "MMMMMMMM>>>>>>>>>MM>>>>>>>>MMMMMM>>>>>>>>>>>>>>>>>MMM!>>>>>>>>>>>>>>>>!MMM>>>>>>>>>MM>>>>>>>>MMMMMMM\n"
        "MMMMMMMM>>>>>>>>>MM>>>>>>>>MMM>>>>>>>>>>>>>>>>!MMM>>>>>>>>>>>>>>>>>MMMM>MM>>>>>>>>>MM>>>>>>>>MMMMMMM\n"
        "MMMMMMMM>>>>>>>>>MM>>>>>>>>MM>>>>>>>>>>>>>>MMMM>>>>>>>>>>>>>>>>MMMM>>>>>MM>>>>>>>>>MM>>>>>>>>MMMMMMM\n"
        "MMMMMMMM>>>>>>>>>MM>>>>>>>>MM>>>>>>>>>>;MMM>>>>>>>>>>>>>>>>!MMM>>>>>>>>>MM>>>>>>>>>MM>>>>>>>>MMMMMMM\n"
        "MMMMMMMM>>>>>>>>>MM>>>>>>>>MM>>>>>>MMMMM>>>>>>>>>>>>>>>>MMMM>!MMMM>>>>>>MM>>>>>>>>>MM>>>>>>>>MMMMMMM\n"
        "MMMMMMMM>>>>>>>>>MM>>>>>>>>MM>>>>MMMM>>>>>>>>>>>>>>>>MMMMM>>>>>>>MMM>>>>MM>>>>>>>>>MM>>>>>>>>MMMMMMM\n"
        "MMMMMMMM!>>>>>>>>MM>>>>>>>>MMMMMM!>>>>>>>>>>>>>>>>MMM!>>>>>>>>>>>>>>MMMMMM>>>>>>>>>MM>>>>>>>!MMMMMMM\n"
        "MMMMMMMM>>>>>>>>>MM>>>>>>>>MMM>>>>>>>>>>>>>>>>>MMMMMM!>>>>>>>>>>>>>>>>>>MM>>>>>>>>>MM>>>>>>>!MMMMMMM\n"
        "MMMMMMMMMMMM>>>>>MM>>>>>>>>MM>>>>>>>>>>>>>>MMMMMMMMMMMMMM>>>>>>>>>>>>>>>MM>>>>>>>>>MM>>>>>MMMMMMMMMM\n"
        "MMMMMMMMMMMMMMM>>MM>>>>>>>>MM>>>>>>>>>!>MMMMMMMMMMMMMMMMMMMM!>>>>>>>>>>>MM>>>>>>>>>M!>MMMMMMMMMMMMMM\n"
        "MMMMMMMMMMMMMMMMMMM>>>>>>>>MM>>>>>>MMMMMMMMMMMMMMMMMMMMMMMMMMMMMM>>>>>>>MM>>>>>>>>>MMMMMMMMMMMMMMMMM\n"
        "MMMMMMMMMMMMMMMMMMMMMMM>>>>MM>>>MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM>>>>MM>>>>MMMMMMMMMMMMMMMMMMMMMM\n"
        "MMMMMMMMMMMMMMMMMMMMMMMMMM!MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM>MMMMMMMMMMMMMMMMMMMMMMMMM";
const std::string OFFSET = "        ";
