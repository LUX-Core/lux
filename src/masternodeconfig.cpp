#include "net.h"
#include "masternodeconfig.h"
#include "util.h"
#include "chainparams.h"

CMasternodeConfig masternodeConfig;

void CMasternodeConfig::add(std::string alias, std::string ip, std::string privKey, std::string txHash, std::string outputIndex) 
{
    CMasternodeEntry cme(alias, ip, privKey, txHash, outputIndex);
    entries.push_back(cme);
}

bool CMasternodeConfig::read(std::string& strErr) 
{
    
    int linenumber = 1;
    boost::filesystem::path pathMasternodeConfigFile = GetMasternodeConfigFile();
    boost::filesystem::ifstream streamConfig(pathMasternodeConfigFile);

    if (!streamConfig.good()) 
    {
        FILE* configFile = fopen(pathMasternodeConfigFile.string().c_str(), "a");
        if (configFile != NULL) 
        {
            std::string strHeader = "# Masternode config file\n"
                                    "# Format: alias IP:port masternodeprivkey collateral_output_txid collateral_output_index\n"
                                    "# Example: mn1 127.0.0.2:26868 93HaYBVUCYjEMeeH1Y4sBGLALQZE1Yc1K64xiqgX37tGBDQL8Xg 2bcd3c84c84f87eaa86e4e56834c92927a07f9e18718810b92e0d0324456a67c 0\n";
            fwrite(strHeader.c_str(), std::strlen(strHeader.c_str()), 1, configFile);
            fclose(configFile);
        }
        return true; // Nothing to read, so just return
    }
	
    for (std::string line; std::getline(streamConfig, line); linenumber++) 
        {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string comment, alias, ip, privKey, txHash, outputIndex;

        if (iss >> comment) {
            if (comment.at(0) == '#') continue;
            iss.str(line);
            iss.clear();
        }    

	if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) 
        {
            iss.str(line);
            iss.clear();
            if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) 
            {
                LogPrintf("CMasternodeConfig::read - Could not parse masternode.conf. Line: %s\n", line.c_str());
                streamConfig.close();
                return false;
            }
        }

        if (Params().NetworkID() == CBaseChainParams::MAIN) 
	{
            if(CService(ip).GetPort() != 26868) 
	    {
            LogPrintf("Invalid port detected in masternode.conf: %s (must be26868 for mainnet)\n", line.c_str());
            streamConfig.close();
            return false;
            }
        } 
	else if(CService(ip).GetPort() == 26868) 
	{
            LogPrintf("Invalid port detected in masternode.conf: %s (26868 must be only on mainnet)\n", line.c_str());
            streamConfig.close();
            return false;
        }

        if (!(CService(ip).IsIPv4() && CService(ip).IsRoutable())) 
	{
            LogPrintf("Invalid Address detected in masternode.conf: %s (IPV4 ONLY) \n", line.c_str());
            streamConfig.close();
            return false;
        }

        add(alias, ip, privKey, txHash, outputIndex);
    }
    streamConfig.close();
    return true;
}

bool CMasternodeConfig::CMasternodeEntry::castOutputIndex(int &n)
{
    try {
        n = std::stoi(outputIndex);
    } catch (const std::exception e) {
        LogPrintf("%s: %s on getOutputIndex\n", __func__, e.what());
        return false;
    }

    return true;
}
