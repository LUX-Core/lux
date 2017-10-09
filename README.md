<b> LUX coin - New Implemented PHI1612 PoW/PoS Hybrid Algorithm & Parallel Masternode </b><br /><br />
======================================================================================================

©LUX 2017 | by 216k155 - @Dra
----------------------------

http://luxcoin.tech/

The next generation GPU mining hash is here.

Based on a new PHI1612 PoW/PoS Hybrid Algorithm.

-----------------------------

Lux Detail:
-----------
lux.conf
	server=1

	daemon=1

	listen=1

	rpcuser=your-user-name

	rpcpassword=your-password

	rpcport=9898

	rpcallowip=127.0.0.1

	staking=1

PoW rewards
-----------
    int64_t GetProofOfWorkReward(int64_t nFees, int nHeight)
	{
    int64_t nSubsidy = 1 * COIN;

    if(pindexBest->nHeight == 1)
    {
        nSubsidy = 3000000 * COIN;                     // Initilised static pre-mine. 
    }
        else if(pindexBest->nHeight < 500)             // First halving - Activated instamine protection 
    {
        nSubsidy = 1 * COIN;  // ~500
    }
        else if(pindexBest->nHeight < 1000000)         // Second halving - Initilised normal blockchain
    {
        nSubsidy = 10 * COIN;  // ~10m
    }
        else if(pindexBest->nHeight < 1001000)         // Third halving - Superblock rewards | Happy Birthday Lux 1 Year | 10/10/2018 | 30 LUX/block reward 
    {
        nSubsidy = 30 * COIN;  // ~30,000 reward to miner
    }
        else if(pindexBest->nHeight < 5000000)         // Last halving - Re-activate normal blockchain
    {
        nSubsidy = 10 * COIN;  // ~10m
    }
		else if(pindexBest->nHeight < 6000000) // PoW end block 6m - Reduce block reward | Automatic initilised new blockchain after 6m blocks 
    {
        nSubsidy = 10 * COIN;  // ~10m
    }
        else
    {
        nSubsidy = 1 * COIN; 
    }

    LogPrint("creation", "GetProofOfWorkReward() : create=%s nSubsidy=%d nHeight=%d\n", FormatMoney(nSubsidy), nSubsidy, nHeight);

    return nSubsidy + nFees;
	} 

PoS rewards
-----------
	// Miner's coin stake reward based on coin age spent (coin-days)
	int64_t GetProofOfStakeReward(int64_t nCoinAge, int64_t nFees, int nHeight)
	{
    int64_t nSubsidy = 1 * COIN;

    if(pindexBest->nHeight < 100000) // First 100,000 blocks double stake for masternode ready
    {
        nSubsidy = 2 * COIN;
    }
        else
    {
        nSubsidy = 1 * COIN;
    }


    LogPrint("creation", "GetProofOfStakeReward(): create=%s nCoinAge=%d nHeight=%d\n", FormatMoney(nSubsidy), nCoinAge, nHeight);

    return nSubsidy + nFees;
	}

Masternodes Requirement:
------------------------
	static const int64_t DARKSEND_COLLATERAL = (16120*COIN);    //161.20 LUX 
	static const int64_t DARKSEND_FEE = (0.002*COIN);
	static const int64_t DARKSEND_POOL_MAX = (1999999.99*COIN);

===========================

01/10/2017 -----> PHI1612 hash initilized, replaced novacoin-qt, json, add intallised-dependencies.sh, changed qt/res,etc....

02/10/2017 -----> Added echo hash into PHI1612

03/10/2017 -----> New Phi1612 hashing algorithm completed
Note: Not finished yet, still need some work. 

04/10/2017 ------> testnet trial version released

06/10/2017 ------> Final test before launch ( 06/10/2017 - 09/10/2017)

07/10/2017 ------> Changed bhcoin to lux ( Trial Version v3)

09/10/2017 ------> Updated New Trial Version Lux - PHI | PoW/PoS Hybrid - Parallel Masternode Implemented | I2pd Networks Remote Activation

-------------------------------------------------------------------

# Install
=========
-Unix daemon compile:

	cd lux

	sudo sh install-dependencies.sh install

	cd src/leveldb

	chmod +x build_detect_platform

	make clean && make libleveldb.a libmemenv.a

	cd ..

	make -f makefile.unix 

-Unix QT wallet compile:

	cd lux

	qmake USE_QRCODE=1

	make

