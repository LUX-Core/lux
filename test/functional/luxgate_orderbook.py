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
    connect_nodes_bi,
    p2p_port,
    rpc_port,
    wait_until,
)
import os
import json
import decimal
import time

def EncodeDecimal(o):
    if isinstance(o, decimal.Decimal):
            return str(o)
    raise TypeError(repr(o) + " is not JSON serializable")

class ExampleTest(LuxTestFramework):

    def set_test_params(self):

        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [["-connect=0"], ["-connect=0"]]

    def setup_chain(self):
        super().setup_chain()

        self.setup_configs(0)
        self.setup_configs(1)

    def setup_configs(self, node_index):
        with open(os.path.join(self.options.tmpdir+"/node" + str(node_index), "lux.conf"), 'w', encoding='utf8') as f:
            f.write("rpcuser=rpcuser\n")
            f.write("testnet=1\n")
            f.write("bind=127.0.0.1\n")
            f.write("port=%s\n" % p2p_port(node_index))
            f.write("rpcport=%s\n" % (rpc_port(node_index)))
            f.write("rpcpassword=rpcpassword\n")

        with open(os.path.join(self.options.tmpdir+"/node" + str(node_index), "luxgate.json"), 'w', encoding='utf8') as f:
            postdata = json.dumps(
                {'coins': [
                    {
                        "ticker": "BTC",
                        "host": "127.0.0.1",
                        "port": 8332, 
                        "rpcuser": "rpcuser",
                        "rpcpassword": "rpcpassword",
                        "zmq_pub_raw_tx_endpoint": "127.0.0.1:5000"
                    },
                ]},
                default=EncodeDecimal, ensure_ascii=True)
            f.write(postdata)

    def setup_network(self):
        self.setup_nodes()


    def run_test(self):

        node0 = self.nodes[0]
        node1 = self.nodes[1]

        assert_equal(0, len(node0.rpc.listorderbook()['orders']))
        assert_equal(0, len(node1.rpc.listorderbook()['orders']))

        connect_nodes(node1, 0, True)

        node1.rpc.createorder('LUX', 'BTC', '2', '1')
        assert_equal(1, len([x for x in node1.rpc.listorderbook()['orders'] if x['status'] == 'new']))

        node0.rpc.createorder('BTC', 'LUX', '1', '2')

        time.sleep(2.0) # wait for p2p exchange

        assert_equal(0, len(node0.rpc.listorderbook()['orders']))
        assert_equal(1, len([x for x in node1.rpc.listorderbook()['orders'] if x['status'] == 'contract_ack']))




if __name__ == '__main__':
    ExampleTest().main()
