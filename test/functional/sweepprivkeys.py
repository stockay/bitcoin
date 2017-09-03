#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the sweepprivkeys RPC."""

import decimal
import math

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_fee_amount

class SweepPrivKeysTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 2

    def check_balance(self, delta, txid):
        node = self.nodes[0]
        new_balance = node.getbalance('*', 0)
        balance_change = new_balance - self.balance
        actual_fee = delta - balance_change
        tx_vsize = node.getrawtransaction(txid, True)['vsize']
        assert_fee_amount(actual_fee, tx_vsize, self.tx_feerate)
        self.balance = new_balance

    def run_test(self):
        node = self.nodes[0]
        miner = self.nodes[1]

        keys = (
            ('mkckmmfVv89sW1HUjyRuydGhwFmSaYtRvG', '92YkaycAxLPUqbbV78V9nNngKLnyVd9T8uZuZAzQnc26dJSP4fm'),
            ('mw8s1FS2Vr7GwQF8bnDVUQHQZq5qWqz5kq', '93VijJgAYnVUGXAfxYhbMHVGVwQUEXK1YnPvcCod3x1RLbzUhXe'),
        )

        # This test is not meant to test fee estimation and we'd like
        # to be sure all txs are sent at a consistent desired feerate
        self.tx_feerate = self.nodes[0].getnetworkinfo()['relayfee'] * 2
        node.settxfee(self.tx_feerate)

        miner.generate(120)
        self.sync_all()
        self.balance = node.getbalance('*', 0)

        txid = node.sendtoaddress(keys[0][0], 10)
        self.check_balance(-10, txid)

        # Sweep from mempool
        import json
        txid = node.sweepprivkeys({'privkeys': (keys[0][1],), 'label': 'test 1'})
        assert_equal(node.listtransactions()[-1]['label'], 'test 1')
        self.check_balance(10, txid)

        txid = node.sendtoaddress(keys[1][0], 5)
        self.check_balance(-5, txid)
        self.sync_all()
        miner.generate(4)
        self.sync_all()
        assert_equal(self.balance, node.getbalance('*', 1))

        # Sweep from blockchain
        txid = node.sweepprivkeys({'privkeys': (keys[1][1],), 'label': 'test 2'})
        assert_equal(node.listtransactions()[-1]['label'], 'test 2')
        self.check_balance(5, txid)

if __name__ == '__main__':
    SweepPrivKeysTest().main()
