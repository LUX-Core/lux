// Copyright (c) 2012-2013 giv
// Copyright (c) 2017-2018 orignal
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//--------------------------------------------------------------------------------------------------
#ifndef I2P_H
#define I2P_H

#include "util.h"
#include "i2psam/i2psam.h"

#define I2P_NET_NAME_PARAM              "-i2p"

#define I2P_SESSION_NAME_PARAM          "-i2psessionname"
#define I2P_SESSION_NAME_DEFAULT        "Luxcore-client"

#define I2P_SAM_HOST_PARAM              "-samhost"
#define I2P_SAM_HOST_DEFAULT            SAM_DEFAULT_ADDRESS

#define I2P_SAM_PORT_PARAM              "-samport"
#define I2P_SAM_PORT_DEFAULT            SAM_DEFAULT_PORT

#define I2P_SAM_MY_DESTINATION_PARAM    "-mydestination"
#define I2P_SAM_MY_DESTINATION_DEFAULT  SAM_GENERATE_MY_DESTINATION

#define I2P_SAM_I2P_OPTIONS_PARAM       "-i2poptions"
#define I2P_SAM_I2P_OPTIONS_DEFAULT     SAM_DEFAULT_I2P_OPTIONS

#define I2P_SAM_GENERATE_DESTINATION_PARAM "-generatei2pdestination"

namespace SAM
{

class StreamSessionAdapter
{
public:

    StreamSessionAdapter();
    ~StreamSessionAdapter();

	bool Start ();
	void Stop ();

    SAM::SOCKET accept(bool silent);
    SAM::SOCKET connect(const std::string& destination, bool silent);
    bool forward(const std::string& host, uint16_t port, bool silent);
    std::string namingLookup(const std::string& name) const;
    SAM::FullDestination destGenerate() const;

    void stopForwarding(const std::string& host, uint16_t port);
    void stopForwardingAll();

    const SAM::FullDestination& getMyDestination() const;

    const sockaddr_in& getSAMAddress() const;
    const std::string& getSAMHost() const;
              uint16_t getSAMPort() const;
    const std::string& getNickname() const;
    const std::string& getSAMMinVer() const;
    const std::string& getSAMMaxVer() const;
    const std::string& getSAMVersion() const;
    const std::string& getOptions() const;

private:

	bool StartSession(
			const std::string& nickname,
            const std::string& SAMHost       = SAM_DEFAULT_ADDRESS,
                  uint16_t     SAMPort       = SAM_DEFAULT_PORT,
            const std::string& myDestination = SAM_GENERATE_MY_DESTINATION,
            const std::string& i2pOptions    = SAM_DEFAULT_I2P_OPTIONS,
            const std::string& minVer        = SAM_DEFAULT_MIN_VER,
            const std::string& maxVer        = SAM_DEFAULT_MAX_VER);
	void StopSession ();
	

private:
    class SessionHolder;

    std::shared_ptr<SessionHolder> sessionHolder_;
};

} // namespace SAM

class I2PSession : private SAM::StreamSessionAdapter
{
public:
    // In C++11 this code is thread safe, in C++03 it isn't
    static SAM::StreamSessionAdapter& Instance()
    {
        static I2PSession i2pSession;
        return i2pSession;
    }

    static std::string GenerateB32AddressFromDestination(const std::string& destination);

private:
    I2PSession();
    ~I2PSession();

    I2PSession(const I2PSession&);
    I2PSession& operator=(const I2PSession&);
};

#endif // I2P_H
