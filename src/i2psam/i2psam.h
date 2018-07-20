// Copyright (c) 2012-2013 giv
// Copyright (c) 2017-2018 orignal
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//--------------------------------------------------------------------------------------------------
// EdDSA assumed
#ifndef I2PSAM_H
#define I2PSAM_H

#include <string>
#include <list>
#include <stdint.h>
#include <memory>
#include <utility>
#include <ostream>

#ifdef WIN32
//#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN 1
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>     // for sockaddr_in
#include <arpa/inet.h>      // for ntohs and htons
#endif

// TODO: check a possible bug about cast -1 to SOCKET
#define SAM_INVALID_SOCKET      (-1)
#define SAM_SOCKET_ERROR        (-1)

#define SAM_DEFAULT_ADDRESS         "127.0.0.1"
#define SAM_DEFAULT_PORT            7656
#define SAM_DEFAULT_MIN_VER         "3.0"
#define SAM_DEFAULT_MAX_VER         "3.0"
#define SAM_GENERATE_MY_DESTINATION "TRANSIENT"
#define SAM_MY_NAME                 "ME"
#define SAM_DEFAULT_I2P_OPTIONS     ""

#define SAM_NAME_INBOUND_QUANTITY           "inbound.quantity"
#define SAM_DEFAULT_INBOUND_QUANTITY        2
#define SAM_NAME_INBOUND_LENGTH             "inbound.length"
#define SAM_DEFAULT_INBOUND_LENGTH          2
#define SAM_NAME_INBOUND_LENGTHVARIANCE     "inbound.lengthVariance"
#define SAM_DEFAULT_INBOUND_LENGTHVARIANCE  0
#define SAM_NAME_INBOUND_BACKUPQUANTITY     "inbound.backupquantity"
#define SAM_DEFAULT_INBOUND_BACKUPQUANTITY  0
#define SAM_NAME_INBOUND_ALLOWZEROHOP       "inbound.allowzerohop"
#define SAM_DEFAULT_INBOUND_ALLOWZEROHOP    true
#define SAM_NAME_INBOUND_IPRESTRICTION      "inbound.iprestriction"
#define SAM_DEFAULT_INBOUND_IPRESTRICTION   2
#define SAM_NAME_OUTBOUND_QUANTITY          "outbound.quantity"
#define SAM_DEFAULT_OUTBOUND_QUANTITY       2
#define SAM_NAME_OUTBOUND_LENGTH            "outbound.length"
#define SAM_DEFAULT_OUTBOUND_LENGTH         2
#define SAM_NAME_OUTBOUND_LENGTHVARIANCE    "outbound.lengthvariance"
#define SAM_DEFAULT_OUTBOUND_LENGTHVARIANCE 0
#define SAM_NAME_OUTBOUND_BACKUPQUANTITY    "outbound.backupquantity"
#define SAM_DEFAULT_OUTBOUND_BACKUPQUANTITY 0
#define SAM_NAME_OUTBOUND_ALLOWZEROHOP      "outbound.allowzerohop"
#define SAM_DEFAULT_OUTBOUND_ALLOWZEROHOP   true
#define SAM_NAME_OUTBOUND_IPRESTRICTION     "outbound.iprestriction"
#define SAM_DEFAULT_OUTBOUND_IPRESTRICTION  2
#define SAM_NAME_OUTBOUND_PRIORITY          "outbound.priority"
#define SAM_DEFAULT_OUTBOUND_PRIORITY       0

namespace SAM
{

typedef int SOCKET;

class Message
{
public:
    enum SessionStyle
    {
        sssStream,
        sssDatagram,    // not supported now
        sssRaw          // not supported now
    };

    enum eStatus
    {
        OK,
        EMPTY_ANSWER,
        CLOSED_SOCKET,
        CANNOT_PARSE_ERROR,

    // The destination is already in use
    //
    // ->  SESSION CREATE ...
    // <-  SESSION STATUS RESULT=DUPLICATED_DEST
        DUPLICATED_DEST,

    // The nickname is already associated with a session
    //
    // ->  SESSION CREATE ...
    // <-  SESSION STATUS RESULT=DUPLICATED_ID
        DUPLICATED_ID,

    // A generic I2P error (e.g. I2CP disconnection, etc.)
    //
    // ->  HELLO VERSION ...
    // <-  HELLO REPLY RESULT=I2P_ERROR MESSAGE={$message}
    //
    // ->  SESSION CREATE ...
    // <-  SESSION STATUS RESULT=I2P_ERROR MESSAGE={$message}
    //
    // ->  STREAM CONNECT ...
    // <-  STREAM STATUS RESULT=I2P_ERROR MESSAGE={$message}
    //
    // ->  STREAM ACCEPT ...
    // <-  STREAM STATUS RESULT=I2P_ERROR MESSAGE={$message}
    //
    // ->  STREAM FORWARD ...
    // <-  STREAM STATUS RESULT=I2P_ERROR MESSAGE={$message}
    //
    // ->  NAMING LOOKUP ...
    // <-  NAMING REPLY RESULT=INVALID_KEY NAME={$name} MESSAGE={$message}
        I2P_ERROR,

    // Stream session ID doesn't exist
    //
    // ->  STREAM CONNECT ...
    // <-  STREAM STATUS RESULT=INVALID_ID MESSAGE={$message}
    //
    // ->  STREAM ACCEPT ...
    // <-  STREAM STATUS RESULT=INVALID_ID MESSAGE={$message}
    //
    // ->  STREAM FORWARD ...
    // <-  STREAM STATUS RESULT=INVALID_ID MESSAGE={$message}
        INVALID_ID,

    // The destination is not a valid private destination key
    //
    // ->  SESSION CREATE ...
    // <-  SESSION STATUS RESULT=INVALID_KEY MESSAGE={$message}
    //
    // ->  STREAM CONNECT ...
    // <-  STREAM STATUS RESULT=INVALID_KEY MESSAGE={$message}
    //
    // ->  NAMING LOOKUP ...
    // <-  NAMING REPLY RESULT=INVALID_KEY NAME={$name} MESSAGE={$message}
        INVALID_KEY,

    // The peer exists, but cannot be reached
    //
    // ->  STREAM CONNECT ...
    // <-  STREAM STATUS RESULT=CANT_REACH_PEER MESSAGE={$message}
        CANT_REACH_PEER,

    // Timeout while waiting for an event (e.g. peer answer)
    //
    // ->  STREAM CONNECT ...
    // <-  STREAM STATUS RESULT=TIMEOUT MESSAGE={$message}
        TIMEOUT,

    // The SAM bridge cannot find a suitable version
    //
    // ->  HELLO VERSION ...
    // <-  HELLO REPLY RESULT=NOVERSION MESSAGE={$message}
        NOVERSION,

    // The naming system can't resolve the given name
    //
    // ->  NAMING LOOKUP ...
    // <-  NAMING REPLY RESULT=INVALID_KEY NAME={$name} MESSAGE={$message}
        KEY_NOT_FOUND,

    // The peer cannot be found on the network
    //
    // ??
        PEER_NOT_FOUND,

    // ??
    //
    // ->  STREAM ACCEPT
    // <-  STREAM STATUS RESULT=ALREADY_ACCEPTING
        ALREADY_ACCEPTING,

        // ??
        FAILED,
        // ??
        CLOSED
    };

    template<class T>
    struct Answer
    {
        const Message::eStatus status;
        T value;

        Answer(Message::eStatus status, const T& value)
            : status(status), value(value) {}
        explicit Answer(Message::eStatus status)
            : status(status), value() {}
    };

    static std::string hello(const std::string& minVer, const std::string& maxVer);
    static std::string sessionCreate(SessionStyle style, const std::string& sessionID, const std::string& nickname, const std::string& destination = SAM_GENERATE_MY_DESTINATION, const std::string& options = "");
    static std::string streamAccept(const std::string& sessionID, bool silent = false);
    static std::string streamConnect(const std::string& sessionID, const std::string& destination, bool silent = false);
    static std::string streamForward(const std::string& sessionID, const std::string& host, uint16_t port, bool silent = false);

    static std::string namingLookup(const std::string& name);
    static std::string destGenerate();

    static eStatus checkAnswer(const std::string& answer);
    static std::string getValue(const std::string& answer, const std::string& key);
private:
    static std::string createSAMRequest(const char* format, ...);
};

class Socket
{
public:
    Socket(const std::string& SAMHost, uint16_t SAMPort, const std::string &minVer, const std::string& maxVer);
    Socket(const sockaddr_in& addr, const std::string& minVer, const std::string& maxVer);
    // explicit because we don't want to create any socket implicity
    explicit Socket(const Socket& rhs); // creates a new socket with the same parameters
    ~Socket();

    void write(const std::string& msg);
    std::string read();
    SOCKET release();
    void close();

    bool isOk() const;

    const std::string& getVersion() const;
    const std::string& getHost() const;
    uint16_t getPort() const;
    const std::string& getMinVer() const;
    const std::string& getMaxVer() const;

    const sockaddr_in& getAddress() const;

private:
    SOCKET socket_;
    sockaddr_in servAddr_;
    std::string SAMHost_;
    uint16_t SAMPort_;
    const std::string minVer_;
    const std::string maxVer_;
    std::string version_;

#ifdef WIN32
    static int instances_;
    static void initWSA();
    static void freeWSA();
#endif

    void handshake();
    void init();

    Socket& operator=(const Socket&);
};

struct FullDestination
{
    std::string pub;
    std::string priv;
    bool isGenerated;

    FullDestination() {}
    FullDestination(const std::string& pub, const std::string& priv, bool isGenerated)
        :pub(pub), priv(priv), isGenerated(isGenerated) {}
};

template<class T>
struct RequestResult
{
    bool isOk;
    T value;

    RequestResult()
        : isOk(false) {}

    explicit RequestResult(const T& value)
        : isOk(true), value(value) {}
};

template<class T>
struct RequestResult<std::shared_ptr<T> >
{
    // a class-helper for resolving a problem with conversion from temporary RequestResult to non-const RequestResult&
    struct RequestResultRef
    {
        bool isOk;
        T* value;

        RequestResultRef(bool isOk, T* value)
            : isOk(isOk), value(value) {}
    };

    bool isOk;
    std::shared_ptr<T> value;

    RequestResult()
        : isOk(false) {}

    explicit RequestResult(std::shared_ptr<T>& value)
        : isOk(true), value(value) {}


    // some C++ magic
    RequestResult(RequestResultRef ref)
        : isOk(ref.isOk), value(ref.value) {}

    RequestResult& operator=(RequestResultRef ref)
    {
        if (value.get() != ref.value)
        {
            isOk = ref.isOk;
            value.reset(ref.value);
        }
        return *this;
    }

    operator RequestResultRef()
    {
        return RequestResultRef(this->isOk, this->value.release());
    }
};

template<>
struct RequestResult<void>
{
    bool isOk;

    RequestResult()
        : isOk(false) {}

    explicit RequestResult(bool isOk)
        : isOk(isOk) {}
};

class StreamSession
{
public:
    StreamSession(
            const std::string& nickname,
            const std::string& SAMHost     = SAM_DEFAULT_ADDRESS,
                  uint16_t     SAMPort     = SAM_DEFAULT_PORT,
            const std::string& destination = SAM_GENERATE_MY_DESTINATION,
            const std::string& i2pOptions  = SAM_DEFAULT_I2P_OPTIONS,
            const std::string& minVer      = SAM_DEFAULT_MIN_VER,
            const std::string& maxVer      = SAM_DEFAULT_MAX_VER);
    explicit StreamSession(StreamSession& rhs);
    ~StreamSession();

    static std::string generateSessionID();

    RequestResult<std::shared_ptr<Socket> > accept(bool silent);
    RequestResult<std::shared_ptr<Socket> > connect(const std::string& destination, bool silent);
    RequestResult<void> forward(const std::string& host, uint16_t port, bool silent);
    RequestResult<const std::string> namingLookup(const std::string& name) const;
    RequestResult<const FullDestination> destGenerate() const;

    void stopForwarding(const std::string& host, uint16_t port);
    void stopForwardingAll();

    const FullDestination& getMyDestination() const;

    const sockaddr_in& getSAMAddress() const;
    const std::string& getSAMHost() const;
              uint16_t getSAMPort() const;
    const std::string& getNickname() const;
    const std::string& getSessionID() const;
    const std::string& getSAMMinVer() const;
    const std::string& getSAMMaxVer() const;
    const std::string& getSAMVersion() const;
    const std::string& getOptions() const;

    bool isSick() const;
	bool isReady () const { return socket_.isOk() && !isSick (); };

	static std::ostream& getLogStream ();
	static void SetLogFile (const std::string& filename);
	static void CloseLogFile ();

private:
    StreamSession(const StreamSession& rhs);
    StreamSession& operator=(const StreamSession& rhs);

    struct ForwardedStream
    {
        Socket* socket;
        std::string host;
        uint16_t port;
        bool silent;

        ForwardedStream(Socket* socket, const std::string& host, uint16_t port, bool silent)
            : socket(socket), host(host), port(port), silent(silent) {}
    };

    typedef std::list<ForwardedStream> ForwardedStreamsContainer;

    Socket socket_;
    const std::string nickname_;
    const std::string sessionID_;
    FullDestination myDestination_;
    const std::string i2pOptions_;
    ForwardedStreamsContainer forwardedStreams_;
    mutable bool isSick_;
	static std::shared_ptr<std::ostream> logStream;

    void fallSick() const;
    FullDestination createStreamSession(const std::string &destination);

    static Message::Answer<const std::string> rawRequest(Socket& socket, const std::string& requestStr);
    static Message::Answer<const std::string> request(Socket& socket, const std::string& requestStr, const std::string& keyOnSuccess);
    static Message::eStatus request(Socket& socket, const std::string& requestStr);
    // commands
    static Message::Answer<const std::string> createStreamSession(Socket& socket, const std::string& sessionID, const std::string& nickname, const std::string& destination, const std::string& options);
    static Message::Answer<const std::string> namingLookup(Socket& socket, const std::string& name);
    static Message::Answer<const FullDestination> destGenerate(Socket& socket);

    static Message::eStatus accept(Socket& socket, const std::string& sessionID, bool silent);
    static Message::eStatus connect(Socket& socket, const std::string& sessionID, const std::string& destination, bool silent);
    static Message::eStatus forward(Socket& socket, const std::string& sessionID, const std::string& host, uint16_t port, bool silent);
};

} // namespace SAM

#endif // I2PSAM_H
