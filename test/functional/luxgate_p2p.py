#!/usr/bin/env python3
# Copyright (c) 2018 LUX Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Luxgate P2P exchanging test

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


class LuxgateNode(NodeConnCB):

    def __init__(self):
        super().__init__()
        self.received_orders = []
    
    def on_createorder(self, conn, message):
        self.received_orders.append(message.order)


def EncodeDecimal(o):
    if isinstance(o, decimal.Decimal):
            return str(o)
    raise TypeError(repr(o) + " is not JSON serializable")

class LuxgateTest(LuxTestFramework):

    def set_test_params(self):

        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [["-connect=0"]]

    def setup_chain(self):
        super().setup_chain()
        self.setup_configs(0)

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
                        "rpcpassword": "rpcpassword"
                    },
                ]},
                default=EncodeDecimal, ensure_ascii=True)
            f.write(postdata)

    def setup_network(self):
        self.setup_nodes()


    def run_test(self):
        '''
        Start 1 node and send testing messages to it
        '''

        node1 = self.nodes[0]

        # Create connecting with  the mininode

        node0 = LuxgateNode()
        connections = []
        connections.append(NodeConn(dstaddr='127.0.0.1', dstport=p2p_port(0), rpc=node1, callback=node0, net='testnet4'))
        node0.add_connection(connections[0])
        NetworkThread().start()
        node0.wait_for_verack()

        # Veifying that rpc for creating order works

        node1.rpc.createorder('LUX', 'BTC', '2', '1')
        node0.wait_for_createorder()

        assert_equal(b'LUX', node0.received_orders[0].base)
        assert_equal(b'BTC', node0.received_orders[0].rel)


if __name__ == '__main__':
    LuxgateTest().main()
