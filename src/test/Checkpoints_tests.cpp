//
// Unit tests for block-chain checkpoints
//
#include <boost/assign/list_of.hpp> // for 'map_list_of()'
#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>

#include "../checkpoints.h"
#include "../util.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(Checkpoints_tests)

BOOST_AUTO_TEST_CASE(sanity)
{
    uint256 p11111 = uint256("0x");
    uint256 p134444 = uint256("0x");
    BOOST_CHECK(Checkpoints::CheckBlock(0, p0));
  

    
    // Wrong hashes at checkpoints should fail:
    BOOST_CHECK(!Checkpoints::CheckBlock(0, p0));
   

    // ... but any hash not at a checkpoint should succeed:
    BOOST_CHECK(Checkpoints::CheckBlock(0+1, p0));
  

    BOOST_CHECK(Checkpoints::GetTotalBlocksEstimate() >= 0);
}    

BOOST_AUTO_TEST_SUITE_END()
