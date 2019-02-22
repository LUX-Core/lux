// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The Luxcore developers
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


