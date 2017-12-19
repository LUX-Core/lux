// Copyright (c) 2017 The LUX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "accumulators.h"
#include "accumulatormap.h"
#include "chainparams.h"
#include "main.h"
#include "txdb.h"
#include "init.h"
#include "spork.h"

using namespace libzerocoin;

std::map<uint32_t, CBigNum> mapAccumulatorValues;
std::list<uint256> listAccCheckpointsNoDB;

uint32_t ParseChecksum(uint256 nChecksum, CoinDenomination denomination)
{
    //shift to the beginning bit of this denomination and trim any remaining bits by returning 32 bits only
    int pos = distance(zerocoinDenomList.begin(), find(zerocoinDenomList.begin(), zerocoinDenomList.end(), denomination));
    nChecksum = nChecksum >> (32*((zerocoinDenomList.size() - 1) - pos));
    return nChecksum.Get32();
}

uint32_t GetChecksum(const CBigNum &bnValue)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << bnValue;
    uint256 hash = Hash(ss.begin(), ss.end());

    return hash.Get32();
}

bool GetAccumulatorValueFromChecksum(uint32_t nChecksum, bool fMemoryOnly, CBigNum& bnAccValue)
{
    if (mapAccumulatorValues.count(nChecksum)) {
        bnAccValue = mapAccumulatorValues.at(nChecksum);
        return true;
    }

    if (fMemoryOnly)
        return false;

    if (!zerocoinDB->ReadAccumulatorValue(nChecksum, bnAccValue)) {
        bnAccValue = 0;
    }

    return true;
}

bool GetAccumulatorValueFromDB(uint256 nCheckpoint, CoinDenomination denom, CBigNum& bnAccValue)
{
    uint32_t nChecksum = ParseChecksum(nCheckpoint, denom);
    return GetAccumulatorValueFromChecksum(nChecksum, false, bnAccValue);
}

void AddAccumulatorChecksum(const uint32_t nChecksum, const CBigNum &bnValue, bool fMemoryOnly)
{
    if(!fMemoryOnly)
        zerocoinDB->WriteAccumulatorValue(nChecksum, bnValue);
    mapAccumulatorValues.insert(make_pair(nChecksum, bnValue));
}

void DatabaseChecksums(AccumulatorMap& mapAccumulators)
{
    uint256 nCheckpoint = 0;
    for (auto& denom : zerocoinDenomList) {
        CBigNum bnValue = mapAccumulators.GetValue(denom);
        uint32_t nCheckSum = GetChecksum(bnValue);
        AddAccumulatorChecksum(nCheckSum, bnValue, false);
        nCheckpoint = nCheckpoint << 32 | nCheckSum;
    }
}

bool EraseChecksum(uint32_t nChecksum)
{
    //erase from both memory and database
    mapAccumulatorValues.erase(nChecksum);
    return zerocoinDB->EraseAccumulatorValue(nChecksum);
}

bool EraseAccumulatorValues(const uint256& nCheckpointErase, const uint256& nCheckpointPrevious)
{
    for (auto& denomination : zerocoinDenomList) {
        uint32_t nChecksumErase = ParseChecksum(nCheckpointErase, denomination);
        uint32_t nChecksumPrevious = ParseChecksum(nCheckpointPrevious, denomination);

        //if the previous checksum is the same, then it should remain in the database and map
        if(nChecksumErase == nChecksumPrevious)
            continue;

        if (!EraseChecksum(nChecksumErase))
            return false;
    }

    return true;
}

bool LoadAccumulatorValuesFromDB(const uint256 nCheckpoint)
{
    for (auto& denomination : zerocoinDenomList) {
        uint32_t nChecksum = ParseChecksum(nCheckpoint, denomination);

        //if read is not successful then we are not in a state to verify zerocoin transactions
        CBigNum bnValue;
        if (!zerocoinDB->ReadAccumulatorValue(nChecksum, bnValue)) {
            LogPrint("zero","%s : Missing databased value for checksum %d\n", __func__, nChecksum);
            if (!count(listAccCheckpointsNoDB.begin(), listAccCheckpointsNoDB.end(), nCheckpoint))
                listAccCheckpointsNoDB.push_back(nCheckpoint);
            return false;
        }
        mapAccumulatorValues.insert(make_pair(nChecksum, bnValue));
    }
    return true;
}

//Erase accumulator checkpoints for a certain block range
bool EraseCheckpoints(int nStartHeight, int nEndHeight)
{
    if (chainActive.Height() < nStartHeight)
        return false;

    nEndHeight = min(chainActive.Height(), nEndHeight);

    CBlockIndex* pindex = chainActive[nStartHeight];
    uint256 nCheckpointPrev = pindex->pprev->nAccumulatorCheckpoint;

    //Keep a list of checkpoints from the previous block so that we don't delete them
    list<uint32_t> listCheckpointsPrev;
    for (auto denom : zerocoinDenomList)
        listCheckpointsPrev.emplace_back(ParseChecksum(nCheckpointPrev, denom));

    while (true) {
        uint256 nCheckpointDelete = pindex->nAccumulatorCheckpoint;

        for (auto denom : zerocoinDenomList) {
            uint32_t nChecksumDelete = ParseChecksum(nCheckpointDelete, denom);
            if (count(listCheckpointsPrev.begin(), listCheckpointsPrev.end(), nCheckpointDelete))
                continue;
            EraseChecksum(nChecksumDelete);
        }
        LogPrintf("%s : erasing checksums for block %d\n", __func__, pindex->nHeight);

        if (pindex->nHeight + 1 <= nEndHeight)
            pindex = chainActive.Next(pindex);
        else
            break;
    }

    return true;
}

//Get checkpoint value for a specific block height
bool CalculateAccumulatorCheckpoint(int nHeight, uint256& nCheckpoint)
{
    if (nHeight < Params().Zerocoin_StartHeight()) {
        nCheckpoint = 0;
        return true;
    }

    //the checkpoint is updated every ten blocks, return current active checkpoint if not update block
    if (nHeight % 10 != 0) {
        nCheckpoint = chainActive[nHeight - 1]->nAccumulatorCheckpoint;
        return true;
    }

    //set the accumulators to last checkpoint value
    AccumulatorMap mapAccumulators;
    if (!mapAccumulators.Load(chainActive[nHeight - 1]->nAccumulatorCheckpoint)) {
        if (chainActive[nHeight - 1]->nAccumulatorCheckpoint == 0) {
            //Before zerocoin is fully activated so set to init state
            mapAccumulators.Reset();
        } else {
            LogPrintf("%s: failed to reset to previous checkpoint\n", __func__);
            return false;
        }
    }

    //Whether this should filter out invalid/fraudulent outpoints
    bool fFilterInvalid = nHeight >= Params().Zerocoin_Block_RecalculateAccumulators();

    //Accumulate all coins over the last ten blocks that havent been accumulated (height - 20 through height - 11)
    int nTotalMintsFound = 0;
    CBlockIndex *pindex = chainActive[nHeight - 20];

    //On a specific block, a recalculation of the accumulators will be forced
    if (nHeight == Params().Zerocoin_Block_RecalculateAccumulators()) {
        pindex = chainActive[Params().Zerocoin_Block_LastGoodCheckpoint() - 10];
        mapAccumulators.Reset();
        if (!mapAccumulators.Load(chainActive[Params().Zerocoin_Block_LastGoodCheckpoint()]->nAccumulatorCheckpoint)) {
            LogPrintf("%s: failed to reset to previous checkpoint when recalculating accumulators\n", __func__);
            return false;
        }
        LogPrintf("*** %s recalculating checkpoint\n", __func__);

        // Erase the checkpoints from the period of time that bad mints were being made
        if (!EraseCheckpoints(Params().Zerocoin_Block_LastGoodCheckpoint() + 1, nHeight)) {
            LogPrintf("%s : failed to erase Checkpoints while recalculating checkpoints\n", __func__);
            return false;
        }
    }

    while (pindex->nHeight < nHeight - 10) {
        // checking whether we should stop this process due to a shutdown request
        if (ShutdownRequested()) {
            return false;
        }

        //make sure this block is eligible for accumulation
        if (pindex->nHeight < Params().Zerocoin_StartHeight()) {
            pindex = chainActive[pindex->nHeight + 1];
            continue;
        }

        //grab mints from this block
        CBlock block;
        if(!ReadBlockFromDisk(block, pindex)) {
            LogPrint("zero","%s: failed to read block from disk\n", __func__);
            return false;
        }

        std::list<PublicCoin> listPubcoins;
        if (!BlockToPubcoinList(block, listPubcoins, fFilterInvalid)) {
            LogPrint("zero","%s: failed to get zerocoin mintlist from block %n\n", __func__, pindex->nHeight);
            return false;
        }

        nTotalMintsFound += listPubcoins.size();
        LogPrint("zero", "%s found %d mints\n", __func__, listPubcoins.size());

        //add the pubcoins to accumulator
        for (const PublicCoin pubcoin : listPubcoins) {
            if(!mapAccumulators.Accumulate(pubcoin, true)) {
                LogPrintf("%s: failed to add pubcoin to accumulator at height %n\n", __func__, pindex->nHeight);
                return false;
            }
        }
        pindex = chainActive.Next(pindex);
    }

    // if there were no new mints found, the accumulator checkpoint will be the same as the last checkpoint
    if (nTotalMintsFound == 0) {
        nCheckpoint = chainActive[nHeight - 1]->nAccumulatorCheckpoint;
    }
    else
        nCheckpoint = mapAccumulators.GetCheckpoint();

    // make sure that these values are databased because reorgs may have deleted the checksums from DB
    DatabaseChecksums(mapAccumulators);

    LogPrint("zero", "%s checkpoint=%s\n", __func__, nCheckpoint.GetHex());
    return true;
}

bool InvalidCheckpointRange(int nHeight)
{
    return nHeight > Params().Zerocoin_Block_LastGoodCheckpoint() && nHeight < Params().Zerocoin_Block_RecalculateAccumulators();
}

bool GenerateAccumulatorWitness(const PublicCoin &coin, Accumulator& accumulator, AccumulatorWitness& witness, int nSecurityLevel, int& nMintsAdded, string& strError)
{
    uint256 txid;
    if (!zerocoinDB->ReadCoinMint(coin.getValue(), txid)) {
        LogPrint("zero","%s failed to read mint from db\n", __func__);
        return false;
    }

    CTransaction txMinted;
    uint256 hashBlock;
    if (!GetTransaction(txid, txMinted, hashBlock)) {
        LogPrint("zero","%s failed to read tx\n", __func__);
        return false;
    }

    int nHeightMintAdded= mapBlockIndex[hashBlock]->nHeight;
    uint256 nCheckpointBeforeMint = 0;
    CBlockIndex* pindex = chainActive[nHeightMintAdded];
    int nChanges = 0;

    //find the checksum when this was added to the accumulator officially, which will be two checksum changes later
    //reminder that checksums are generated when the block height is a multiple of 10
    while (pindex->nHeight < chainActive.Tip()->nHeight - 1) {
        if (pindex->nHeight == nHeightMintAdded) {
            pindex = chainActive[pindex->nHeight + 1];
            continue;
        }

        //check if the next checksum was generated
        if (pindex->nHeight % 10 == 0) {
            nChanges++;

            if (nChanges == 1) {
                nCheckpointBeforeMint = pindex->nAccumulatorCheckpoint;
                break;
            }
        }
        pindex = chainActive.Next(pindex);
    }

    //the height to start accumulating coins to add to witness
    int nAccStartHeight = nHeightMintAdded - (nHeightMintAdded % 10);

    //If the checkpoint is from the recalculated checkpoint period, then adjust it
    int nHeight_LastGoodCheckpoint = Params().Zerocoin_Block_LastGoodCheckpoint();
    int nHeight_Recalculate = Params().Zerocoin_Block_RecalculateAccumulators();
    if (pindex->nHeight < nHeight_Recalculate - 10 && pindex->nHeight > nHeight_LastGoodCheckpoint) {
        //The checkpoint before the mint will be the last good checkpoint
        nCheckpointBeforeMint = chainActive[nHeight_LastGoodCheckpoint]->nAccumulatorCheckpoint;
        nAccStartHeight = nHeight_LastGoodCheckpoint - 10;
    }

    //Get the accumulator that is right before the cluster of blocks containing our mint was added to the accumulator
    CBigNum bnAccValue = 0;
    if (GetAccumulatorValueFromDB(nCheckpointBeforeMint, coin.getDenomination(), bnAccValue)) {
        if (bnAccValue > 0) {
            accumulator.setValue(bnAccValue);
            witness.resetValue(accumulator, coin);
        }
    }

    //security level: this is an important prevention of tracing the coins via timing. Security level represents how many checkpoints
    //of accumulated coins are added *beyond* the checkpoint that the mint being spent was added too. If each spend added the exact same
    //amounts of checkpoints after the mint was accumulated, then you could know the range of blocks that the mint originated from.
    if (nSecurityLevel < 100) {
        //add some randomness to the user's selection so that it is not always the same
        nSecurityLevel += CBigNum::randBignum(10).getint();

        //security level 100 represents adding all available coins that have been accumulated - user did not select this
        if (nSecurityLevel >= 100)
            nSecurityLevel = 99;
    }

    //add the pubcoins (zerocoinmints that have been published to the chain) up to the next checksum starting from the block
    pindex = chainActive[nAccStartHeight];
    int nChainHeight = chainActive.Height();
    int nHeightStop = nChainHeight % 10;
    nHeightStop = nChainHeight - nHeightStop - 20; // at least two checkpoints deep
    int nCheckpointsAdded = 0;
    nMintsAdded = 0;
    while (pindex->nHeight < nHeightStop + 1) {
        if (pindex->nHeight != nAccStartHeight && pindex->pprev->nAccumulatorCheckpoint != pindex->nAccumulatorCheckpoint)
            ++nCheckpointsAdded;

        //if a new checkpoint was generated on this block, and we have added the specified amount of checkpointed accumulators,
        //then initialize the accumulator at this point and break
        if (!InvalidCheckpointRange(pindex->nHeight) && (pindex->nHeight >= nHeightStop || (nSecurityLevel != 100 && nCheckpointsAdded >= nSecurityLevel))) {
            uint32_t nChecksum = ParseChecksum(chainActive[pindex->nHeight + 10]->nAccumulatorCheckpoint, coin.getDenomination());
            CBigNum bnAccValue = 0;
            if (!zerocoinDB->ReadAccumulatorValue(nChecksum, bnAccValue)) {
                LogPrintf("%s : failed to find checksum in database for accumulator\n", __func__);
                return false;
            }
            accumulator.setValue(bnAccValue);
            break;
        }

        // if this block contains mints of the denomination that is being spent, then add them to the witness
        if (pindex->MintedDenomination(coin.getDenomination())) {
            //grab mints from this block
            CBlock block;
            if(!ReadBlockFromDisk(block, pindex)) {
                LogPrintf("%s: failed to read block from disk while adding pubcoins to witness\n", __func__);
                return false;
            }

            list<PublicCoin> listPubcoins;
            if(!BlockToPubcoinList(block, listPubcoins, true)) {
                LogPrintf("%s: failed to get zerocoin mintlist from block %n\n", __func__, pindex->nHeight);
                return false;
            }

            //add the mints to the witness
            for (const PublicCoin pubcoin : listPubcoins) {
                if (pubcoin.getDenomination() != coin.getDenomination())
                    continue;

                if (pindex->nHeight == nHeightMintAdded && pubcoin.getValue() == coin.getValue())
                    continue;

                witness.addRawValue(pubcoin.getValue());
                ++nMintsAdded;
            }
        }

        pindex = chainActive[pindex->nHeight + 1];
    }

    if (nMintsAdded < Params().Zerocoin_RequiredAccumulation()) {
        strError = _(strprintf("Less than %d mints added, unable to create spend", Params().Zerocoin_RequiredAccumulation()).c_str());
        LogPrintf("%s : %s\n", __func__, strError);
        return false;
    }

    // calculate how many mints of this denomination existed in the accumulator we initialized
    int nZerocoinStartHeight = GetZerocoinStartHeight();
    pindex = chainActive[nZerocoinStartHeight];
    while (pindex->nHeight < nAccStartHeight) {
        nMintsAdded += count(pindex->vMintDenominationsInBlock.begin(), pindex->vMintDenominationsInBlock.end(), coin.getDenomination());
        pindex = chainActive[pindex->nHeight + 1];
    }

    LogPrint("zero","%s : %d mints added to witness\n", __func__, nMintsAdded);
    return true;
}