// Copyright (c) 2015-2017 The Bitcoin Core developers
// Copyright (c) 2017 The LUX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Functionality for communicating with LuxGate.
 */
#ifndef BITCOIN_LUXCONTROL_H
#define BITCOIN_LUXCONTROL_H

#include <string>

#include <boost/function.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/thread.hpp>

extern const std::string DEFAULT_LUX_CONTROL;
static const bool DEFAULT_LISTEN_ONION = true;

void StartLuxControl(boost::thread_group& threadGroup);
void InterruptLuxControl();
void StopLuxControl();

#endif /* BITCOIN_LUXCONTROL_H */


