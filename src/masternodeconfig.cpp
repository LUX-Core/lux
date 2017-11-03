
#include "net.h"
#include "masternodeconfig.h"
#include "util.h"

CMasternodeConfig masternodeConfig;

void CMasternodeConfig::add(std::string alias, std::string ip, std::string privKey, std::string txHash, std::string outputIndex) {
    CMasternodeEntry cme(alias, ip, privKey, txHash, outputIndex);
    entries.push_back(cme);
}

bool CMasternodeConfig::read(boost::filesystem::path path) {
    boost::filesystem::ifstream streamConfig(GetMasternodeConfigFile());
    if (!streamConfig.good()) {
        return true; // No masternode.conf file is OK
    }

    for(std::string line; std::getline(streamConfig, line); )
    {
        if(line.empty()) {
            continue;
        }
        std::istringstream iss(line);
        std::string alias, ip, privKey, txHash, outputIndex;
        if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) {
            LogPrintf("CMasternodeConfig::read - Could not parse masternode.conf. Line: %s\n", line.c_str());
            streamConfig.close();
            return false;
        }

        if(Params().NetworkID() == CChainParams::MAIN){
            if(CService(ip).GetPort() != 28666) {
                LogPrintf("Invalid port detected in masternode.conf: %s (must be 17170 for mainnet)\n", line.c_str());
                streamConfig.close();
                return false;
            }
        } else if(CService(ip).GetPort() == 28666) {
            LogPrintf("Invalid port detected in masternode.conf: %s (17170 must be only on mainnet)\n", line.c_str());
            streamConfig.close();
            return false;
        }

        if (!(CService(ip).IsIPv4() && CService(ip).IsRoutable())) {
            LogPrintf("Invalid Address detected in masternode.conf: %s (IPV4 ONLY) \n", line.c_str());
            streamConfig.close();
            return false;
        }

        add(alias, ip, privKey, txHash, outputIndex);
    }
}
