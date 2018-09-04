#!/usr/bin/env python3
# Copyright (c) 2018 LUX Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Luxgate test

The test needs running bitcoind(testnet) instance with opts specified in luxgate.json
"""
# Imports should be in PEP8 ordering (std library first, then third party
# libraries then local imports).
from collections import defaultdict

# Avoid wildcard * imports if possible
from test_framework.blocktools import (create_block, create_coinbase)
from test_framework.mininode import (
    CInv,
    NetworkThread,
    NodeConn,
    NodeConnCB,
    mininode_lock,
    msg_block,
    msg_getdata,
)
from test_framework.test_framework import LuxTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes,
    p2p_port,
    rpc_port,
    wait_until,
)
import os
import json
import decimal


def EncodeDecimal(o):
    if isinstance(o, decimal.Decimal):
            return str(o)
    raise TypeError(repr(o) + " is not JSON serializable")

class ExampleTest(LuxTestFramework):

    def set_test_params(self):

        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [[], []]



    def setup_chain(self):
        super().setup_chain()

        with open(os.path.join(self.options.tmpdir+"/node0", "lux.conf"), 'w', encoding='utf8') as f:
            f.write("rpcuser=rpcuser\n")
            f.write("testnet=1\n")
            f.write("port=%s\n" % p2p_port(0))
            f.write("rpcport=%s\n" % (rpc_port(0)))
            f.write("rpcpassword=rpcpassword\n")

        with open(os.path.join(self.options.tmpdir+"/node0", "luxgate.json"), 'w', encoding='utf8') as f:
            postdata = json.dumps(
                {'coins': [
                    {
                        "ticker": "BTC",
                        "host": "127.0.0.1",
                        "port": 8332, # testnet port
                        "rpcuser": "rpcuser",
                        "rpcpassword": "rpcpassword"
                    },
                ]},
                default=EncodeDecimal, ensure_ascii=True)
            f.write(postdata)

        with open(os.path.join(self.options.tmpdir+"/node1", "lux.conf"), 'w', encoding='utf8') as f:
            f.write("rpcuser=rpcuser\n")
            f.write("testnet=1\n")
            f.write("port=%s\n" % p2p_port(1))
            f.write("rpcport=%s\n" % (rpc_port(1)))
            f.write("rpcpassword=rpcpassword\n")

        with open(os.path.join(self.options.tmpdir+"/node1", "luxgate.json"), 'w', encoding='utf8') as f:
            postdata = json.dumps(
                {'coins': [
                    {
                        "ticker": "BTC",
                        "host": "127.0.0.1",
                        "port": 8339, # wrong port
                        "rpcuser": "rpcuser",
                        "rpcpassword": "rpcpassword"
                    },
                ]},
                default=EncodeDecimal, ensure_ascii=True)
            f.write(postdata)


    def setup_network(self):
        self.setup_nodes()


    def run_test(self):
        """
        Start 2 nodes:
         - 1st is working
         - 2nd has wrong port for btc in config
         Check that luxd responds with actual statuses
        """

        node0 = self.nodes[0]

        get_coins_rs = node0.rpc.getactivecoins()
        btc = get_coins_rs['coins'][0]
        assert_equal(btc['ticker'], 'BTC')
        assert_equal(btc['rpchost'], '127.0.0.1')
        assert_equal(btc['rpcport'], 8332)
        
        assert_equal(btc['active'], True)
        assert_equal(btc['swap_supported'], True)

        #
        # Test wrong connection
        #

        node1 = self.nodes[1]
        get_coins_rs = node1.rpc.getactivecoins()
        btc = get_coins_rs['coins'][0]
        assert_equal(btc['ticker'], 'BTC')
        assert_equal(btc['rpchost'], '127.0.0.1')
        assert_equal(btc['rpcport'], 8339)

        assert_equal(btc['active'], False)
        assert_equal(btc['swap_supported'], False)
        assert_equal(btc['errors'][0], 'couldn\'t connect to server')

        #
        # Check that orderbook is empty on start and at least works
        #

        orderbook_rs = node0.rpc.listorderbook()
        assert_equal(0, len(orderbook_rs['orders']))
        



if __name__ == '__main__':
    ExampleTest().main()
