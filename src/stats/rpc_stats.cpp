// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc/server.h"
#include "stats/stats.h"
#include "util.h"
#include "utilstrencodings.h"

#include <stdint.h>

#include <univalue.h>

UniValue getmempoolstats(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "getmempoolstats\n"
            "\nReturns the collected mempool statictics (non-linear non-interpolated samples).\n"
            "\nResult:\n"
            "  {\n"
            "    \"time_from\" : \"timestamp\",     (numeric) Timestamp, first sample\n"
            "    \"time_to\"   : \"timestamp\",     (numeric) Timestamp, last sample\n"
            "    \"samples\"   : [\n"
            "                    [<delta_in_secs>,<tx_count>,<dynamic_mem_usage>,<min_fee_per_k>],\n"
            "                    [<delta_in_secs>,<tx_count>,<dynamic_mem_usage>,<min_fee_per_k>],\n"
            "                    ...\n"
            "                  ]\n"
            "  }\n"
            "\nExamples:\n" +
            HelpExampleCli("getmempoolstats", "") + HelpExampleRpc("getmempoolstats", ""));

    // get stats from the core stats model
    uint64_t timeFrom = 0;
    uint64_t timeTo = 0;
    mempoolSamples_t samples = CStats::DefaultStats()->mempoolGetValuesInRange(timeFrom, timeTo);

    // use "flat" json encoding for performance reasons
    UniValue samplesObj(UniValue::VARR);
    for (struct CStatsMempoolSample& sample : samples) {
        UniValue singleSample(UniValue::VARR);
        singleSample.push_back(UniValue((uint64_t)sample.timeDelta));
        singleSample.push_back(UniValue(sample.txCount));
        singleSample.push_back(UniValue(sample.dynMemUsage));
        singleSample.push_back(UniValue(sample.minFeePerK));
        samplesObj.push_back(singleSample);
    }

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("time_from", timeFrom));
    result.push_back(Pair("time_to", timeTo));
    result.push_back(Pair("samples", samplesObj));

    return result;
}

static const CRPCCommand commands[] =
{
    //  category              name                      actor (function)         okSafeMode
    //  --------------------- ------------------------  -----------------------  ----------
        {"stats",             "getmempoolstats",        &getmempoolstats,        true},

};

void RegisterStatsRPCCommands(CRPCTable& tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
