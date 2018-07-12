#!/usr/bin/env python3

from test_framework.test_framework import LuxTestFramework
from test_framework.util import *
from test_framework.script import *
from test_framework.mininode import *
from test_framework.lux import *
from test_framework.luxconfig import *

class LuxNullSenderTest(LuxTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def run_test(self):
        self.node = self.nodes[0]
        self.node.generate(10+COINBASE_MATURITY)
        tx = CTransaction()
        tx.vin = [make_vin(self.node, COIN + 1000000)]
        tx.vout = [CTxOut(int(COIN), CScript([OP_TRUE]))]
        tx_hex = self.node.signrawtransaction(bytes_to_hex_str(tx.serialize()))['hex']
        parent_tx_id = self.node.sendrawtransaction(tx_hex)
        self.node.generate(1)

        tx = CTransaction()
        tx.vin = [CTxIn(COutPoint(int(parent_tx_id, 16), 0), scriptSig=CScript([]), nSequence=0)]
        tx.vout = [CTxOut(0, CScript([b"\x04", CScriptNum(1000000), CScriptNum(LUX_MIN_GAS_PRICE), b"\x00", OP_CREATE]))]
        tx_hex = bytes_to_hex_str(tx.serialize())
        assert_raises_rpc_error(-26, 'bad-txns-invalid-sender-script', self.node.sendrawtransaction, tx_hex)
        block_count = self.node.getblockcount()
        self.node.generate(1)
        assert_equal(self.node.getblockcount(), block_count+1)

if __name__ == '__main__':
    LuxNullSenderTest().main()
