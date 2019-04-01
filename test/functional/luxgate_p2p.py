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
from test_framework.mininode import *
from test_framework.test_framework import LuxTestFramework
from test_framework.util import *
import os
import json
import decimal
import time


class LuxgateNode(NodeConnCB):

    def __init__(self):
        super().__init__()
        self.received_orders = []
        self.his_address = None
        self.his_tx = None
    
    def on_createorder(self, conn, message):
        self.received_orders.append(message.order)

    def on_reqswap(self, conn, message):
        self.his_address = message.address

    def on_reqswapack(self, conn, message): pass

    def on_swccreated(self, conn, message):
        self.his_tx = message.txid

    def on_swcack(self, conn, message): pass

    def on_reject(self, conn, message):
        raise AssertionError("Message rejected: %s" % repr(message))

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
                        "rpcpassword": "rpcpassword",
                        "zmq_pub_raw_tx_endpoint": "127.0.0.1:5000"
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
        assert_equal(200000000, node0.received_orders[0].base_amount)
        assert_equal(100000000, node0.received_orders[0].rel_amount)

        assert_equal('new', node1.rpc.listorderbook()['orders'][0]['status'])

        # create matching order
        my_order = LGOrder(
                base=b'BTC', rel=b'LUX', 
                base_amount=100000000, rel_amount=200000000, 
                sender=node0.received_orders[0].sender)

        node0.send_message(msg_ordermatch(my_order))
        node0.wait_for_reqswap()

        assert_equal('swap_requested', node1.rpc.listorderbook()['orders'][0]['status'])

        print('Received address : %s' % node0.his_address)

        node0.send_message(msg_reqswapack(my_order, b'LMERZFhojC4MruhonFYZpXsanyRbgynn7r')) # lux testnet
        node0.wait_for_swccreated()

        assert_equal('contract_created', node1.rpc.listorderbook()['orders'][0]['status'])
        print('Received tx : %s' % node0.his_tx)

        # After 10 sec the order must be refunded and removed from orders list
        # Period 10 sec is for testing purposes only
        # Expecting that testing node will have time to restart and refund in 20 sec. 
        start = time.time()
        connections[0].disconnect_node()
        
        # The timer must be active after node restart
        self.stop_nodes()
        time.sleep(1)
        self.start_nodes()

        # Establish connection again
        node0.add_connection(connections[0])
        node0.wait_for_verack()

        # Check that the order is loaded. Assume that the timeout for refund does not has come
        assert_equal(1, len(node1.rpc.listorderbook()['orders']))
        
        while len(node1.rpc.listorderbook()['orders']) != 0 and time.time() - start < 20:
            time.sleep(0.5)

        if time.time() - start > 20: 
            raise AssertionError("Timeout waiting refund event")
        raise AssertionError("Timeout waiting refund event")



if __name__ == '__main__':
    LuxgateTest().main()
