#include "../lux/proposalsagent.h"

//#include <openssl/rsa.h>
//#include <openssl/pem.h>
#include <boost/test/unit_test.hpp>
//#include <iostream>

class TestProposalsAgent : public ProposalsAgent
{
public:
    static const uint256 hashOne;
    static const uint256 hashTwo;

    std::map<uint256, std::list<StorageProposal>> & GetMapProposals()
    {
        return mapProposals;
    }
    std::map<uint256, std::pair<bool, std::time_t>> & GetMapListenProposals()
    {
        return mapListenProposals;
    }

    void AddOneAndTwoListenProposals()
    {
        BOOST_CHECK_EQUAL(GetMapListenProposals().size(), 0);

        auto & proposal1 = GetMapListenProposals()[TestProposalsAgent::hashOne];
        auto & proposal2 = GetMapListenProposals()[TestProposalsAgent::hashTwo];

        proposal1.first = true;
        proposal1.second = std::time(nullptr);
        proposal2.first = true;
        proposal2.second = std::time(nullptr);
    }

    std::vector<uint256> AddOneAndTwoProposals()
    {
        BOOST_CHECK_EQUAL(GetMapProposals().size(), 0);

        std::vector<uint256> result{};

        AddOneAndTwoListenProposals();

        auto & proposal1 = GetMapProposals()[TestProposalsAgent::hashOne];
        auto & proposal2 = GetMapProposals()[TestProposalsAgent::hashTwo];

        proposal1.push_back(MakeProposal(hashOne));
        proposal2.push_back(MakeProposal(hashTwo));

        result.push_back(proposal1.back().GetHash());
        result.push_back(proposal2.back().GetHash());

        return result;
    }

    void AddTestProposal(const StorageProposal &proposal)
    {
        auto & proposals = GetMapProposals()[proposal.orderHash];

        proposals.push_back(proposal);
    }

    static StorageProposal MakeProposal(const uint256 &hash, const CAmount rate = 0, const CService &address = CService{})
    {
        StorageProposal result{};

        result.time = std::time(nullptr);
        result.orderHash = hash;
        result.rate = rate;
        result.address = address;

        return result;
    }
};

std::basic_ostream<char> & operator <<(std::basic_ostream<char> &stream, const uint256 &hash)
{
    stream << hash.ToString();
    return stream;
}

const uint256 TestProposalsAgent::hashOne{uint256S("0000000000000000000000000000000000000000000000000000000000000001")};
const uint256 TestProposalsAgent::hashTwo{uint256S("0000000000000000000000000000000000000000000000000000000000000002")};

BOOST_AUTO_TEST_SUITE(proposal_agent_tests)

BOOST_AUTO_TEST_CASE(listen_proposal)
{
    TestProposalsAgent agent{};

    BOOST_CHECK_EQUAL(agent.GetMapListenProposals().size(), 0);
    agent.ListenProposals(TestProposalsAgent::hashOne);
    BOOST_CHECK_EQUAL(agent.GetMapListenProposals().size(), 1);
    agent.ListenProposals(TestProposalsAgent::hashTwo);
    BOOST_CHECK_EQUAL(agent.GetMapListenProposals().size(), 2);

    BOOST_CHECK_EQUAL(agent.GetMapListenProposals().begin()->first, TestProposalsAgent::hashOne);
    BOOST_CHECK_EQUAL((++agent.GetMapListenProposals().begin())->first, TestProposalsAgent::hashTwo);

    BOOST_CHECK(agent.GetMapListenProposals().begin()->second.first);
    BOOST_CHECK((++agent.GetMapListenProposals().begin())->second.first);
}

BOOST_AUTO_TEST_CASE(stop_listen_proposal)
{
    TestProposalsAgent agent{};

    agent.AddOneAndTwoListenProposals();

    BOOST_CHECK_EQUAL(agent.GetMapListenProposals().size(), 2);
    agent.StopListenProposals(TestProposalsAgent::hashOne);
    BOOST_CHECK_EQUAL(agent.GetMapListenProposals().size(), 2);
    agent.StopListenProposals(TestProposalsAgent::hashTwo);
    BOOST_CHECK_EQUAL(agent.GetMapListenProposals().size(), 2);

    BOOST_CHECK_EQUAL(agent.GetMapListenProposals().begin()->first, TestProposalsAgent::hashOne);
    BOOST_CHECK_EQUAL((++agent.GetMapListenProposals().begin())->first, TestProposalsAgent::hashTwo);

    BOOST_CHECK(!agent.GetMapListenProposals().begin()->second.first);
    BOOST_CHECK(!(++agent.GetMapListenProposals().begin())->second.first);
}

BOOST_AUTO_TEST_CASE(get_listen_proposals)
{
    TestProposalsAgent agent{};

    agent.AddOneAndTwoListenProposals();

    BOOST_CHECK_EQUAL(agent.GetMapListenProposals().size(), 2);
    BOOST_CHECK_EQUAL(agent.GetListenProposals().size(), agent.GetMapListenProposals().size());
    BOOST_CHECK_EQUAL(agent.GetListenProposals()[ 0 ], TestProposalsAgent::hashOne);
    BOOST_CHECK_EQUAL(agent.GetListenProposals()[ 1 ], TestProposalsAgent::hashTwo);
}


//void AddProposal(const StorageProposal &proposal);
//std::vector<StorageProposal> GetProposals(const uint256 &orderHash);
//StorageProposal GetProposal(const uint256 &orderHash, const uint256 &proposalHash);
//void EraseOrdersProposals(const uint256 &orderHash);

BOOST_AUTO_TEST_CASE(add_proposal)
{
    TestProposalsAgent agent{};

    agent.AddOneAndTwoListenProposals();

    BOOST_CHECK_EQUAL(agent.GetMapProposals().size(), 0);
    agent.AddProposal(TestProposalsAgent::MakeProposal(TestProposalsAgent::hashOne));
    BOOST_CHECK_EQUAL(agent.GetMapProposals().size(), 1);
    agent.AddProposal(TestProposalsAgent::MakeProposal(TestProposalsAgent::hashOne));
    BOOST_CHECK_EQUAL(agent.GetMapProposals().size(), 1);
    agent.AddProposal(TestProposalsAgent::MakeProposal(TestProposalsAgent::hashTwo));
    BOOST_CHECK_EQUAL(agent.GetMapProposals().size(), 2);
    agent.AddProposal(TestProposalsAgent::MakeProposal(TestProposalsAgent::hashTwo));
    BOOST_CHECK_EQUAL(agent.GetMapProposals().size(), 2);

    BOOST_CHECK_EQUAL(agent.GetMapProposals().begin()->first, TestProposalsAgent::hashOne);
    BOOST_CHECK_EQUAL((++agent.GetMapProposals().begin())->first, TestProposalsAgent::hashTwo);

    BOOST_CHECK_EQUAL(agent.GetMapProposals().begin()->second.size(), 2);
    BOOST_CHECK_EQUAL((++agent.GetMapProposals().begin())->second.size(), 2);
}

BOOST_AUTO_TEST_CASE(get_proposals)
{
    TestProposalsAgent agent{};

    auto hashes = agent.AddOneAndTwoProposals();

    BOOST_CHECK_EQUAL(agent.GetMapProposals().size(), 2);
    BOOST_CHECK_EQUAL(agent.GetProposals(TestProposalsAgent::hashOne).size(), 1);
    BOOST_CHECK_EQUAL(agent.GetProposals(TestProposalsAgent::hashTwo).size(), 1);
    BOOST_CHECK_EQUAL(agent.GetProposals(TestProposalsAgent::hashOne)[0].orderHash, TestProposalsAgent::hashOne);
    BOOST_CHECK_EQUAL(agent.GetProposals(TestProposalsAgent::hashTwo)[0].orderHash, TestProposalsAgent::hashTwo);
    BOOST_CHECK_EQUAL(agent.GetProposals(TestProposalsAgent::hashOne)[0].GetHash(), hashes[0]);
    BOOST_CHECK_EQUAL(agent.GetProposals(TestProposalsAgent::hashTwo)[0].GetHash(), hashes[1]);
}

BOOST_AUTO_TEST_CASE(get_sorted_proposals)
{
    TestProposalsAgent agent{};

    agent.AddTestProposal(TestProposalsAgent::MakeProposal(TestProposalsAgent::hashOne, 5));
    agent.AddTestProposal(TestProposalsAgent::MakeProposal(TestProposalsAgent::hashOne, 3));
    agent.AddTestProposal(TestProposalsAgent::MakeProposal(TestProposalsAgent::hashOne, 7));
    agent.AddTestProposal(TestProposalsAgent::MakeProposal(TestProposalsAgent::hashOne, 6));
    agent.AddTestProposal(TestProposalsAgent::MakeProposal(TestProposalsAgent::hashOne, 1));

    BOOST_CHECK_EQUAL(agent.GetMapProposals().size(), 1);
    BOOST_CHECK_EQUAL(agent.GetProposals(TestProposalsAgent::hashOne).size(), 5);

    auto proposals = agent.GetSortedProposals(TestProposalsAgent::hashOne);
    BOOST_CHECK_EQUAL(proposals[0].rate, 1);
    BOOST_CHECK_EQUAL(proposals[1].rate, 3);
    BOOST_CHECK_EQUAL(proposals[2].rate, 5);
    BOOST_CHECK_EQUAL(proposals[3].rate, 6);
    BOOST_CHECK_EQUAL(proposals[4].rate, 7);
}

BOOST_AUTO_TEST_CASE(get_proposal)
{
    TestProposalsAgent agent{};

    auto hashes = agent.AddOneAndTwoProposals();

    BOOST_CHECK_EQUAL(agent.GetMapProposals().size(), 2);
    BOOST_CHECK_EQUAL(agent.GetProposal(TestProposalsAgent::hashOne, hashes[0]).orderHash, TestProposalsAgent::hashOne);
    BOOST_CHECK_EQUAL(agent.GetProposal(TestProposalsAgent::hashTwo, hashes[1]).orderHash, TestProposalsAgent::hashTwo);
    BOOST_CHECK_EQUAL(agent.GetProposal(TestProposalsAgent::hashOne, hashes[0]).GetHash(), hashes[0]);
    BOOST_CHECK_EQUAL(agent.GetProposal(TestProposalsAgent::hashTwo, hashes[1]).GetHash(), hashes[1]);

    BOOST_CHECK(agent.GetProposal(TestProposalsAgent::hashOne, hashes[1]).orderHash.IsNull());
    BOOST_CHECK(agent.GetProposal(TestProposalsAgent::hashOne, hashes[1]).time == 0);
    BOOST_CHECK(agent.GetProposal(TestProposalsAgent::hashTwo, hashes[0]).orderHash.IsNull());
    BOOST_CHECK(agent.GetProposal(TestProposalsAgent::hashTwo, hashes[0]).time == 0);
}

BOOST_AUTO_TEST_CASE(erase_orders_proposals)
{
    TestProposalsAgent agent{};

    auto hashes = agent.AddOneAndTwoProposals();

    BOOST_CHECK_EQUAL(agent.GetMapProposals().size(), 2);
    agent.EraseOrdersProposals(TestProposalsAgent::hashOne);
    BOOST_CHECK_EQUAL(agent.GetMapProposals().size(), 1);
    agent.EraseOrdersProposals(TestProposalsAgent::hashTwo);
    BOOST_CHECK_EQUAL(agent.GetMapProposals().size(), 0);
}

BOOST_AUTO_TEST_SUITE_END()
