// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Functionality for communicating with Tor.
 */
#ifndef BITCOIN_TORCONTROL_H
#define BITCOIN_TORCONTROL_H

#include <string>

#include <boost/function.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/thread.hpp>

extern const std::string DEFAULT_TOR_CONTROL;
static const bool DEFAULT_LISTEN_ONION = true;

void StartTorControl(boost::thread_group& threadGroup);
void InterruptTorControl();
void StopTorControl();

#endif /* BITCOIN_TORCONTROL_H */
