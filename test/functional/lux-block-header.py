#!/usr/bin/env python3
# Copyright (c) 2015-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import ComparisonTestFramework
from test_framework.util import *
from test_framework.comptool import TestManager, TestInstance, RejectResult
from test_framework.blocktools import *
from test_framework.mininode import *
from test_framework.address import *
from test_framework.lux import *
import time
from test_framework.key import CECKey
from test_framework.script import *
import struct
import io


def find_unspent(node, amount):
    for unspent in node.listunspent():
        if unspent['amount'] == amount and unspent['spendable']:
            return CTxIn(COutPoint(int(unspent['txid'], 16), unspent['vout']), nSequence=0)
    assert(False)


class LuxBlockHeaderTest(ComparisonTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.tip = None
        self.setup_clean_chain = True

    def run_test(self):
        self.test = TestManager(self, self.options.tmpdir)
        self.test.add_all_connections(self.nodes)
        NetworkThread().start() # Start up network handling in another thread
        self.test.run()

    def get_tests(self):
        # returns a test case that asserts that the current tip was accepted
        def accepted():
            return TestInstance([[self.tip, True]])

        # returns a test case that asserts that the current tip was rejected
        def rejected(reject = None):
            if reject is None:
                return TestInstance([[self.tip, False]])
            else:
                return TestInstance([[self.tip, reject]])

        node = self.nodes[0]
        #mocktime = 1490247077
        #node.setmocktime(mocktime)

        node.generate(10)
        self.block_time = int(time.time())+20
        for i in range(500):
            self.tip = create_block(int(node.getbestblockhash(), 16), create_coinbase(node.getblockcount()+1), self.block_time+i)
            self.tip.solve()
            yield accepted()

        #node.generate(COINBASE_MATURITY+50)
        mocktime = COINBASE_MATURITY+50
        spendable_addresses = []
        # store some addresses to use later
        for unspent in node.listunspent():
            spendable_addresses.append(unspent['address'])

        # first make sure that what is a valid block is accepted
        coinbase = create_coinbase(node.getblockcount()+1)
        coinbase.rehash()
        self.tip = create_block(int(node.getbestblockhash(), 16), coinbase, int(time.time()+mocktime+100))
        self.tip.hashMerkleRoot = self.tip.calc_merkle_root()
        self.tip.solve()
        yield accepted()

        # A block that has an OP_CREATE tx, butwith an incorrect state root
        """
            pragma solidity ^0.4.11;
            contract Test {
                function() payable {}
            }
        """
        tx_hex = node.createcontract("60606040523415600b57fe5b5b60398060196000396000f30060606040525b600b5b5b565b0000a165627a7a72305820693c4900c412f72a51f8c01a36d38d9038d822d953faf5a5b28e40ec6e1a25020029", 1000000, LUX_MIN_GAS_PRICE_STR, spendable_addresses.pop(-1), False)['raw transaction']
        f = io.BytesIO(hex_str_to_bytes(tx_hex))
        tx = CTransaction()
        tx.deserialize(f)

        coinbase = create_coinbase(node.getblockcount()+1)
        coinbase.rehash()
        self.tip = create_block(int(node.getbestblockhash(), 16), coinbase, int(mocktime+200))
        self.tip.vtx.append(tx)
        self.tip.hashMerkleRoot = self.tip.calc_merkle_root()
        self.tip.solve()
        yield rejected()


        # Create a contract for use later.
        """
            pragma solidity ^0.4.11;
            contract Test {
                function() payable {}
            }
        """
        contract_address = node.createcontract("60606040523415600b57fe5b5b60398060196000396000f30060606040525b600b5b5b565b0000a165627a7a72305820693c4900c412f72a51f8c01a36d38d9038d822d953faf5a5b28e40ec6e1a25020029")['address']
        node.generate(1)

        realHashUTXORoot = int(node.getblock(node.getbestblockhash())['hashUTXORoot'], 16)
        realHashStateRoot = int(node.getblock(node.getbestblockhash())['hashStateRoot'], 16)

        # A block with both an invalid hashStateRoot and hashUTXORoot
        coinbase = create_coinbase(node.getblockcount()+1)
        coinbase.rehash()
        self.tip = create_block(int(node.getbestblockhash(), 16), coinbase, int(mocktime+300))
        self.tip.hashUTXORoot = 0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
        self.tip.hashStateRoot = 0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
        self.tip.hashMerkleRoot = self.tip.calc_merkle_root()
        self.tip.solve()
        yield rejected()


        # A block with a tx, but without updated state hashes
        tx_hex = node.sendtocontract(contract_address, "00", 1, 100000, LUX_MIN_GAS_PRICE_STR, spendable_addresses.pop(-1), False)['raw transaction']
        f = io.BytesIO(hex_str_to_bytes(tx_hex))
        tx = CTransaction()
        tx.deserialize(f)

        coinbase = create_coinbase(node.getblockcount()+1)
        coinbase.rehash()
        self.tip = create_block(int(node.getbestblockhash(), 16), coinbase, int(mocktime+400))
        self.tip.realHashUTXORoot = realHashUTXORoot
        self.tip.realHashStateRoot = realHashStateRoot
        self.tip.vtx.append(tx)
        self.tip.hashMerkleRoot = self.tip.calc_merkle_root()
        self.tip.solve()
        yield rejected()

        # A block with an invalid hashUTXORoot
        coinbase = create_coinbase(node.getblockcount()+1)
        coinbase.rehash()
        self.tip = create_block(int(node.getbestblockhash(), 16), coinbase, int(mocktime+500))
        self.tip.hashStateRoot = realHashStateRoot
        self.tip.hashUTXORoot = 0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
        self.tip.hashMerkleRoot = self.tip.calc_merkle_root()
        self.tip.solve()
        yield rejected()

        # A block with an invalid hashStateRoot
        coinbase = create_coinbase(node.getblockcount()+1)
        coinbase.rehash()
        self.tip = create_block(int(node.getbestblockhash(), 16), coinbase, int(mocktime+600))
        self.tip.hashUTXORoot = realHashUTXORoot
        self.tip.hashStateRoot = 0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
        self.tip.hashMerkleRoot = self.tip.calc_merkle_root()
        self.tip.solve()
        yield rejected()

        # Verify that blocks with a correct hashStateRoot and hashUTXORoot are accepted.
        coinbase = create_coinbase(node.getblockcount()+1)
        coinbase.rehash()
        self.tip = create_block(int(node.getbestblockhash(), 16), coinbase, int(mocktime+700))
        self.tip.hashUTXORoot = realHashUTXORoot
        self.tip.hashStateRoot = realHashStateRoot
        self.tip.hashMerkleRoot = self.tip.calc_merkle_root()
        self.tip.solve()
        yield accepted()


if __name__ == '__main__':
    LuxBlockHeaderTest().main()
