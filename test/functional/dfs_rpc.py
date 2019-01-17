#!/usr/bin/env python3
#Copyright (c) 2019 The LUX developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test multiple RPC dfs."""

from test_framework.util import str_to_b64str

import http.client
import json
import os
import time

def assert_equal(v1, v2, message):
    if v1 == v2:
        raise AssertionError(message)

def assert_not_equal(v1, v2, message):
    if v1 != v2:
        raise AssertionError(message)

def assert_less_or_equal(v1, v2, message):
    if v1 <= v2:
        raise AssertionError(message)

class DFSTest ():
    def run_test(self):
        rpcuser = "rpcuser"
        rpcpassword = "rpcpassword"
        authpair = rpcuser + ':' + rpcpassword

        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        conn = http.client.HTTPConnection("localhost", 9777)
        conn.connect()

        ##################################################
        # Check dfsannounce with creating encrypted file #
        ##################################################
        p1 = "../123.jpg"
        p2 = "1"
        p3 = "0.3"

        # failed
        conn.request('POST', '/', '{"method": "dfsannounce", "params": ["%s", "test", "test"] }'%(p1), headers)
        resp = conn.getresponse()
        assert_not_equal(resp.status, 500, "Unexpected success for dfssetparams with params: %s, test, test. Should be fail."%(p1))

        # success
        conn.request('POST', '/', '{"method": "dfsannounce", "params": ["%s", "%s", "%s"] }'%(p1, p2, p3), headers)
        resp = conn.getresponse()
        assert_not_equal(resp.status, 200, "Unexpected fail for dfsannounce with params %s, %s, %s. Should be success."%(p1, p2, p3))

        respBody = resp.read()
        obj = json.loads(respBody)
        uri = obj['result']
        assert_equal(uri, '', "Result is empty")

        # TODO: Add check for exist file on system

        #######################
        # Check dfslistorders #
        #######################

        # success
        conn.request('POST', '/', '{"method": "dfslistorders" }', headers)
        resp = conn.getresponse()
        assert_not_equal(resp.status, 200, "Unexpected fail for dfslistorders. Should be success.")

        respBody = resp.read()
        obj = json.loads(respBody)
        orders = obj['result']

        assert_less_or_equal(len(orders), 0, "Order list is empty")

        founded = False

        for val in orders:
            if val['orderhash'] == uri:
                founded = True

        assert_equal(founded, False, "URI not found")

        ########################
        # Check dfscancelorder #
        ########################

        # failed
        conn.request('POST', '/', '{"method": "dfscancelorder", "params": ["test"] }', headers)
        resp = conn.getresponse()
        respBody = resp.read()
        assert_not_equal(resp.status, 500, "Unexpected success for dfssetparams with params: test. Should be fail.")

        # success
        conn.request('POST', '/', '{"method": "dfscancelorder", "params": ["%s"] }'%(uri), headers)
        resp = conn.getresponse()
        respBody = resp.read()
        assert_not_equal(resp.status, 200, "Unexpected fail for dfscancelorder with params: %s. Should be success."%(uri))

        # success
        conn.request('POST', '/', '{"method": "dfslistorders" }', headers)
        resp = conn.getresponse()

        orders = json.loads(resp.read())['result']

        founded = False

        for val in orders:
            if val['orderhash'] == uri:
                founded = True

        assert_equal(founded, True, "URI found")

        #####################################################################
        # Check dfssetparams & dfsgetinfo & dfssetfolder & dfssettempfolder #
        #####################################################################

        # success
        conn.request('POST', '/', '{"method": "dfssetparams", "params": ["1", "10"] }', headers)
        resp = conn.getresponse()
        resp.read()
        assert_not_equal(resp.status, 200, "Unexpected fail for dfssetparams with params: 1, 10. Should be success.")

        # failed
        conn.request('POST', '/', '{"method": "dfssetparams", "params": ["test", "test"] }', headers)
        resp = conn.getresponse()
        resp.read()
        assert_not_equal(resp.status, 500, "Unexpected success for dfssetparams with params: test, test. Should be fail.")

        # success
        conn.request('POST', '/', '{"method": "dfsgetinfo" }', headers)
        resp = conn.getresponse()
        assert_not_equal(resp.status, 200, "Unexpected fail for dfsgetinfo. Should be success.")
        result = json.loads(resp.read())['result']
        assert_equal(result['rate'], "1", "Rate not changed")
        assert_equal(result['maxblocksgap'], "10", "Max blocks gap not changed")

        # failed
        conn.request('POST', '/', '{"method": "dfssetfolder", "params": ["%s"] }'%(result['dfsfolder'] + "2"), headers)
        resp = conn.getresponse()
        assert_not_equal(resp.status, 500, "Unexpected success for dfssetfolder with params: %s. Should be fail."%(result['dfsfolder'] + "2"))

        # failed
        conn.request('POST', '/', '{"method": "dfssettempfolder", "params": ["%s"] }'%(result['dfstempfolder'] + "2"), headers)
        resp = conn.getresponse()
        resp.read()
        assert_not_equal(resp.status, 500, "Unexpected success for dfssettempfolder with params: %s. Should be fail."%(result['dfstempfolder'] + "2"))

        # success
        conn.request('POST', '/', '{"method": "dfssetfolder", "params": ["%s"] }'%(result['dfsfolder']), headers)
        resp = conn.getresponse()
        resp.read()
        assert_not_equal(resp.status, 200, "Unexpected fail for dfssetfolder with params: %s. Should be success."%(result['dfsfolder']))

        # success
        conn.request('POST', '/', '{"method": "dfssettempfolder", "params": ["%s"] }'%(result['dfstempfolder']), headers)
        resp = conn.getresponse()
        resp.read()
        assert_not_equal(resp.status, 200, "Unexpected fail for dfssettempfolder with params: %s. Should be success."%(result['dfstempfolder']))

        ##########################
        # Check dfslistproposals #
        ##########################

        # success
        p1 = "../123.jpg"
        p2 = "1"
        p3 = "0.3"
        conn.request('POST', '/', '{"method": "dfsannounce", "params": ["%s", "%s", "%s"] }'%(p1, p2, p3), headers)
        resp = conn.getresponse()
        assert_not_equal(resp.status, 200, "Unexpected fail for dfsannounce with params: %s, %s, %s. Should be success."%(p1, p2, p3))

        uri = json.loads(resp.read())['result']

        # failed
        conn.request('POST', '/', '{"method": "dfslistproposals", "params": ["test"] }', headers)
        resp = conn.getresponse()
        resp.read()
        assert_not_equal(resp.status, 500, "Unexpected success for dfslistproposals with params: test. Should be fail.")

        # success
        conn.request('POST', '/', '{"method": "dfslistproposals", "params": ["%s"] }'%(uri), headers)
        resp = conn.getresponse()
        resp.read()
        assert_not_equal(resp.status, 200, "Unexpected fail for dfslistproposals with params: %s. Should be success."%(uri))

        proposal = json.loads(resp.read())['result'][0]

        # ###########################
        # # Check dfsacceptproposal #
        # ###########################

        # failed
        conn.request('POST', '/', '{"method": "dfsacceptproposal", "params": ["test", "test"] }', headers)
        resp = conn.getresponse()
        resp.read()
        assert_not_equal(resp.status, 500, "Unexpected success for dfsacceptproposal with params: test, test. Should be fail.")

        # success
        conn.request('POST', '/', '{"method": "dfsacceptproposal", "params": ["%s", "%s"] }'%(uri, proposal), headers)
        resp = conn.getresponse()
        resp.read()
        assert_not_equal(resp.status, 200, "Unexpected fail for dfsacceptproposal with params: %s %s. Should be success."%(uri, proposal))

        #########################
        # Check dfslocalstorage #
        #########################

        # success
        conn.request('POST', '/', '{"method": "dfslocalstorage" }', headers)
        resp = conn.getresponse()
        resp.read()
        assert_not_equal(resp.status, 200, "Unexpected fail for dfslocalstorage. Should be success.")

        ############################
        # Check dfsremoveoldorders #
        ############################

        # success
        timestamp = int(time.time())
        conn.request('POST', '/', '{"method": "dfsremoveoldorders", "params": ["%s"] }'%(timestamp), headers)
        resp = conn.getresponse()
        resp.read()
        assert_not_equal(resp.status, 200, "Unexpected fail for dfsremoveoldorders with params: %s. Should be success."%(timestamp))

        # failed
        conn.request('POST', '/', '{"method": "dfsremoveoldorders", "params": ["test"] }', headers)
        resp = conn.getresponse()
        resp.read()
        assert_not_equal(resp.status, 500, "Unexpected success for dfsremoveoldorders with params: test. Should be fail.")

        print("All test pass.")
        conn.close()

if __name__ == '__main__':
    DFSTest ().run_test ()
