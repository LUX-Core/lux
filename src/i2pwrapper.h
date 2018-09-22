// Copyright (c) 2012-2013 giv
// Copyright (c) 2017-2018 orignal
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//--------------------------------------------------------------------------------------------------
#ifndef I2PWRAPPER_H
#define I2PWRAPPER_H

#include "util.h"
#include "i2psam/i2psam.h"

#define I2P_SESSION_NAME_DEFAULT        "Luxcore-client"
#define NATIVE_I2P_DESTINATION_SIZE     516
extern char I2PKeydat [1024];

namespace SAM
{
    class StreamSessionAdapter {
    public:
        StreamSessionAdapter(
                const std::string& nickname,
                const std::string& SAMHost       = SAM_DEFAULT_ADDRESS,
                uint16_t     SAMPort       = SAM_DEFAULT_PORT,
                const std::string& myDestination = SAM_GENERATE_MY_DESTINATION,
                const std::string& i2pOptions    = SAM_DEFAULT_I2P_OPTIONS,
                const std::string& minVer        = SAM_DEFAULT_MIN_VER,
                const std::string& maxVer        = SAM_DEFAULT_MAX_VER);

        ~StreamSessionAdapter();

        SAM::SOCKET accept(bool silent);
        SAM::SOCKET connect(const std::string& destination, bool silent);
        bool forward(const std::string& host, uint16_t port, bool silent);
        std::string namingLookup(const std::string& name) const;
        SAM::FullDestination destGenerate() const;
        bool isSick( void )const;

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
        const std::string& getSessionID() const;


    private:
        class SessionHolder;

        std::auto_ptr<SessionHolder> sessionHolder_;
    };

} // namespace SAM

class I2PSession : private SAM::StreamSessionAdapter
{
public:
    // In C++11 this code is thread safe, in C++03 it isn't
    // ToDo: GR Note: Hmm sounds like we need to make this thread safe, or I've not the tools to compile it.
    //       Meeh - Could really use your help on this one....
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

void InitializeI2pSettings( const bool fGenerated );

/**
 * Specific functions we need to implement I2P functionality
 */
std::string FormatI2PNativeFullVersion();
std::string GenerateI2pDestinationMessage( const std::string& pub, const std::string& priv, const std::string& b32, const std::string& configFileName );

struct Identity
{
    uint8_t publicKey[256];
    uint8_t signingKey[128];
    struct
    {
        uint8_t type;
        uint16_t length;
    } certificate;
};

#endif // I2PWRAPPER_H
