// Copyright (c) 2012-2013 giv
// Copyright (c) 2017-2018 orignal
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//--------------------------------------------------------------------------------------------------
// EdDSA assumed
#include "i2psam.h"

#include <iostream>
#include <stdio.h>
#include <string.h>         // for memset
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <fstream>

#ifndef WIN32
#include <errno.h>
#include <unistd.h>
#endif

#ifndef WIN32
#define closesocket         close
#endif

#define SAM_BUFSIZE         65536
#define I2P_DESTINATION_SIZE 521  // EcDSA, GOST and EdDSA, actual size is 524 with trailing AAA== or AAQ==

namespace SAM
{

static void print_error(const std::string& err)
{
#ifdef WIN32
    StreamSession::getLogStream () << err << "(" << WSAGetLastError() << ")" << std::endl;
#else
    StreamSession::getLogStream () << err << "(" << errno << ")" << std::endl;
#endif
}

#ifdef WIN32
int Socket::instances_ = 0;

void Socket::initWSA()
{
    WSADATA wsadata;
    int ret = WSAStartup(MAKEWORD(2,2), &wsadata);
    if (ret != NO_ERROR)
        print_error("Failed to initialize winsock library");
}

void Socket::freeWSA()
{
    WSACleanup();
}
#endif

Socket::Socket(const std::string& SAMHost, uint16_t SAMPort, const std::string& minVer, const std::string &maxVer)
    : socket_(SAM_INVALID_SOCKET), SAMHost_(SAMHost), SAMPort_(SAMPort), minVer_(minVer), maxVer_(maxVer)
{
#ifdef WIN32
    if (instances_++ == 0)
        initWSA();
#endif

    memset(&servAddr_, 0, sizeof(servAddr_));

    servAddr_.sin_family = AF_INET;
    servAddr_.sin_addr.s_addr = inet_addr(SAMHost.c_str());
    servAddr_.sin_port = htons(SAMPort);

    init();
    if (isOk())
        handshake();
}

Socket::Socket(const sockaddr_in& addr, const std::string &minVer, const std::string& maxVer)
    : socket_(SAM_INVALID_SOCKET), servAddr_(addr), minVer_(minVer), maxVer_(maxVer)
{
#ifdef WIN32
    if (instances_++ == 0)
        initWSA();
#endif

    init();
    if (isOk())
        handshake();
}

Socket::Socket(const Socket& rhs)
    : socket_(SAM_INVALID_SOCKET), servAddr_(rhs.servAddr_), minVer_(rhs.minVer_), maxVer_(rhs.maxVer_)
{
#ifdef WIN32
    if (instances_++ == 0)
        initWSA();
#endif

    init();
    if (isOk())
        handshake();
}

Socket::~Socket()
{
    close();

#ifdef WIN32
    if (--instances_ == 0)
        freeWSA();
#endif
}

void Socket::init()
{
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ == SAM_INVALID_SOCKET)
    {
        print_error("Failed to create socket");
        return;
    }

    if (connect(socket_, (const sockaddr*)&servAddr_, sizeof(servAddr_)) == SAM_SOCKET_ERROR)
    {
        close();
        print_error("Failed to connect to SAM");
        return;
    }
}

SOCKET Socket::release()
{
    SOCKET temp = socket_;
    socket_ = SAM_INVALID_SOCKET;
    return temp;
}

void Socket::handshake()
{
    this->write(Message::hello(minVer_, maxVer_));
    const std::string answer = this->read();
    const Message::eStatus answerStatus = Message::checkAnswer(answer);
    if (answerStatus == Message::OK)
        version_ = Message::getValue(answer, "VERSION");
    else
        print_error("Handshake failed");
}

void Socket::write(const std::string& msg)
{
    if (!isOk())
    {
        print_error("Failed to send data because socket is closed");
        return;
    }
    StreamSession::getLogStream () << "Send: " << msg << std::endl;
    ssize_t sentBytes = send(socket_, msg.c_str(), msg.length(), 0);
    if (sentBytes == SAM_SOCKET_ERROR)
    {
        close();
        print_error("Failed to send data");
        return;
    }
    if (sentBytes == 0)
    {
        close();
        print_error("Socket was closed");
        return;
    }
}

std::string Socket::read()
{
    if (!isOk())
    {
        print_error("Failed to read data because socket is closed");
        return std::string();
    }
    char buffer[SAM_BUFSIZE];
    memset(buffer, 0, SAM_BUFSIZE);
    ssize_t recievedBytes = recv(socket_, buffer, SAM_BUFSIZE, 0);
    if (recievedBytes == SAM_SOCKET_ERROR)
    {
        close();
        print_error("Failed to receive data");
        return std::string();
    }
    if (recievedBytes == 0)
    {
        close();
        print_error("Socket was closed");
    }
    StreamSession::getLogStream () << "Reply: " << buffer << std::endl;
    return std::string(buffer);
}

void Socket::close()
{
    if (socket_ != SAM_INVALID_SOCKET)
        ::closesocket(socket_);
    socket_ = SAM_INVALID_SOCKET;
}

bool Socket::isOk() const
{
    return socket_ != SAM_INVALID_SOCKET;
}

const std::string& Socket::getHost() const
{
    return SAMHost_;
}

uint16_t Socket::getPort() const
{
    return SAMPort_;
}

const std::string& Socket::getVersion() const
{
    return version_;
}

const std::string& Socket::getMinVer() const
{
    return minVer_;
}

const std::string& Socket::getMaxVer() const
{
    return maxVer_;
}

const sockaddr_in& Socket::getAddress() const
{
    return servAddr_;
}


//--------------------------------------------------------------------------------------------------

StreamSession::StreamSession(
        const std::string& nickname,
        const std::string& SAMHost     /*= SAM_DEFAULT_ADDRESS*/,
              uint16_t     SAMPort     /*= SAM_DEFAULT_PORT*/,
        const std::string& destination /*= SAM_GENERATE_MY_DESTINATION*/,
        const std::string& i2pOptions  /*= SAM_DEFAULT_I2P_OPTIONS*/,
        const std::string& minVer      /*= SAM_DEFAULT_MIN_VER*/,
        const std::string& maxVer      /*= SAM_DEFAULT_MAX_VER*/)
    : socket_(SAMHost, SAMPort, minVer, maxVer)
    , nickname_(nickname)
    , sessionID_(generateSessionID())
    , i2pOptions_(i2pOptions)
    , isSick_(false)
{
    myDestination_ = createStreamSession(destination);
    getLogStream () << "Created a brand new SAM session (" << sessionID_ << ")" << std::endl;
}

StreamSession::StreamSession(StreamSession& rhs)
    : socket_(rhs.socket_)
    , nickname_(rhs.nickname_)
    , sessionID_(generateSessionID())
    , myDestination_(rhs.myDestination_)
    , i2pOptions_(rhs.i2pOptions_)
    , isSick_(false)
{
    rhs.fallSick();
    rhs.socket_.close();
    (void)createStreamSession(myDestination_.priv);

    for(ForwardedStreamsContainer::const_iterator it = rhs.forwardedStreams_.begin(), end = rhs.forwardedStreams_.end(); it != end; ++it)
        forward(it->host, it->port, it->silent);

    getLogStream () << "Created a new SAM session (" << sessionID_ << ")  from another (" << rhs.sessionID_ << ")" << std::endl;
}

StreamSession::~StreamSession()
{
    stopForwardingAll();
    getLogStream () << "Closing SAM session (" << sessionID_ << ") ..." << std::endl;
}

/*static*/
std::string StreamSession::generateSessionID()
{
    static const int minSessionIDLength = 5;
    static const int maxSessionIDLength = 9;
    static const char sessionIDAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int length = minSessionIDLength - 1;
    std::string result;

    srand(time(NULL));

    while(length < minSessionIDLength)
        length = rand() % maxSessionIDLength;

    while (length-- > 0)
        result += sessionIDAlphabet[rand() % (sizeof(sessionIDAlphabet)-1)];

    return result;
}

RequestResult<std::shared_ptr<Socket> > StreamSession::accept(bool silent)
{
    typedef RequestResult<std::shared_ptr<Socket> > ResultType;

    std::shared_ptr<Socket> streamSocket(new Socket(socket_));
    const Message::eStatus status = accept(*streamSocket, sessionID_, silent);
    switch(status)
    {
    case Message::OK:
        return std::move (RequestResult<std::shared_ptr<Socket> >(streamSocket));
    case Message::EMPTY_ANSWER:
    case Message::CLOSED_SOCKET:
    case Message::INVALID_ID:
    case Message::I2P_ERROR:
        fallSick();
        break;
    default:
        break;
    }
    return ResultType();
}

RequestResult<std::shared_ptr<Socket> > StreamSession::connect(const std::string& destination, bool silent)
{
    typedef RequestResult<std::shared_ptr<Socket> > ResultType;

    std::shared_ptr<Socket> streamSocket(new Socket(socket_));
    const Message::eStatus status = connect(*streamSocket, sessionID_, destination, silent);
    switch(status)
    {
    case Message::OK:
        return ResultType(streamSocket);
    case Message::EMPTY_ANSWER:
    case Message::CLOSED_SOCKET:
    case Message::INVALID_ID:
    case Message::I2P_ERROR:
        fallSick();
        break;
    default:
        break;
    }
    return ResultType();
}

RequestResult<void> StreamSession::forward(const std::string& host, uint16_t port, bool silent)
{
    typedef RequestResult<void> ResultType;

    std::shared_ptr<Socket> newSocket(new Socket(socket_));
    const Message::eStatus status = forward(*newSocket, sessionID_, host, port, silent);
    switch(status)
    {
    case Message::OK:
        forwardedStreams_.push_back(ForwardedStream(newSocket.get(), host, port, silent));
        newSocket = nullptr;    // release after successful push_back only
        return ResultType(true);
    case Message::EMPTY_ANSWER:
    case Message::CLOSED_SOCKET:
    case Message::INVALID_ID:
    case Message::I2P_ERROR:
        fallSick();
        break;
    default:
        break;
    }
    return ResultType();
}

RequestResult<const std::string> StreamSession::namingLookup(const std::string& name) const
{
    typedef RequestResult<const std::string> ResultType;
    typedef Message::Answer<const std::string> AnswerType;

    std::shared_ptr<Socket> newSocket(new Socket(socket_));
    const AnswerType answer = namingLookup(*newSocket, name);
    switch(answer.status)
    {
    case Message::OK:
        return ResultType(answer.value);
    case Message::EMPTY_ANSWER:
    case Message::CLOSED_SOCKET:
        fallSick();
        break;
    default:
        break;
    }
    return ResultType();
}

RequestResult<const FullDestination> StreamSession::destGenerate() const
{
    typedef RequestResult<const FullDestination> ResultType;
    typedef Message::Answer<const FullDestination> AnswerType;

    std::shared_ptr<Socket> newSocket(new Socket(socket_));
    const AnswerType answer = destGenerate(*newSocket);
    switch(answer.status)
    {
    case Message::OK:
        return ResultType(answer.value);
    case Message::EMPTY_ANSWER:
    case Message::CLOSED_SOCKET:
        fallSick();
        break;
    default:
        break;
    }
    return ResultType();
}

FullDestination StreamSession::createStreamSession(const std::string& destination)
{
    typedef Message::Answer<const std::string> AnswerType;

    const AnswerType answer = createStreamSession(socket_, sessionID_, nickname_, destination, i2pOptions_);
    if (answer.status != Message::OK)
    {
        fallSick();
        return FullDestination();
    }
    return FullDestination(answer.value.substr(0, I2P_DESTINATION_SIZE) + "A==", answer.value, (destination == SAM_GENERATE_MY_DESTINATION));
}

void StreamSession::fallSick() const
{
    isSick_ = true;
}

void StreamSession::stopForwarding(const std::string& host, uint16_t port)
{
    for (ForwardedStreamsContainer::iterator it = forwardedStreams_.begin(); it != forwardedStreams_.end(); )
    {
        if (it->port == port && it->host == host)
        {
            delete (it->socket);
            it = forwardedStreams_.erase(it);
        }
        else
            ++it;
    }
}

void StreamSession::stopForwardingAll()
{
    for (ForwardedStreamsContainer::iterator it = forwardedStreams_.begin(); it != forwardedStreams_.end(); ++it)
        delete (it->socket);
    forwardedStreams_.clear();
}

/*static*/
Message::Answer<const std::string> StreamSession::rawRequest(Socket& socket, const std::string& requestStr)
{
    typedef Message::Answer<const std::string> AnswerType;

    if (!socket.isOk())
        return AnswerType(Message::CLOSED_SOCKET);
    socket.write(requestStr);
    const std::string answer = socket.read();
    const Message::eStatus status = Message::checkAnswer(answer);
    return AnswerType(status, answer);
}

/*static*/
Message::Answer<const std::string> StreamSession::request(Socket& socket, const std::string& requestStr, const std::string& keyOnSuccess)
{
    typedef Message::Answer<const std::string> AnswerType;

    const AnswerType answer = rawRequest(socket, requestStr);
    return (answer.status == Message::OK) ?
                AnswerType(answer.status, Message::getValue(answer.value, keyOnSuccess)) :
                answer;
}

/*static*/
Message::eStatus StreamSession::request(Socket& socket, const std::string& requestStr)
{
    return rawRequest(socket, requestStr).status;
}

/*static*/
Message::Answer<const std::string> StreamSession::createStreamSession(Socket& socket, const std::string& sessionID, const std::string& nickname, const std::string& destination, const std::string& options)
{
    return request(socket, Message::sessionCreate(Message::sssStream, sessionID, nickname, destination, options), "DESTINATION");
}

/*static*/
Message::Answer<const std::string> StreamSession::namingLookup(Socket& socket, const std::string& name)
{
    return request(socket, Message::namingLookup(name), "VALUE");
}

/*static*/
Message::Answer<const FullDestination> StreamSession::destGenerate(Socket& socket)
{
// while answer for a DEST GENERATE request doesn't contain a "RESULT" field we parse it manually
    typedef Message::Answer<const FullDestination> ResultType;

    if (!socket.isOk())
        return ResultType(Message::CLOSED_SOCKET, FullDestination());
    socket.write(Message::destGenerate());
    const std::string answer = socket.read();
    const std::string pub = Message::getValue(answer, "PUB");
    const std::string priv = Message::getValue(answer, "PRIV");
    return (!pub.empty() && !priv.empty()) ? ResultType(Message::OK, FullDestination(pub, priv, /*isGenerated*/ true)) : ResultType(Message::EMPTY_ANSWER, FullDestination());
}

/*static*/
Message::eStatus StreamSession::accept(Socket& socket, const std::string& sessionID, bool silent)
{
    return request(socket, Message::streamAccept(sessionID, silent));
}

/*static*/
Message::eStatus StreamSession::connect(Socket& socket, const std::string& sessionID, const std::string& destination, bool silent)
{
    return request(socket, Message::streamConnect(sessionID, destination, silent));
}

/*static*/
Message::eStatus StreamSession::forward(Socket& socket, const std::string& sessionID, const std::string& host, uint16_t port, bool silent)
{
    return request(socket, Message::streamForward(sessionID, host, port, silent));
}

const std::string& StreamSession::getNickname() const
{
    return nickname_;
}

const std::string& StreamSession::getSessionID() const
{
    return sessionID_;
}

const std::string& StreamSession::getOptions() const
{
    return i2pOptions_;
}

const FullDestination& StreamSession::getMyDestination() const
{
    return myDestination_;
}

bool StreamSession::isSick() const
{
    return isSick_;
}

const sockaddr_in& StreamSession::getSAMAddress() const
{
    return socket_.getAddress();
}

const std::string& StreamSession::getSAMHost() const
{
    return socket_.getHost();
}

uint16_t StreamSession::getSAMPort() const
{
    return socket_.getPort();
}

const std::string& StreamSession::getSAMMinVer() const
{
    return socket_.getMinVer();
}

const std::string& StreamSession::getSAMMaxVer() const
{
    return socket_.getMaxVer();
}

const std::string& StreamSession::getSAMVersion() const
{
    return socket_.getVersion();
}

std::shared_ptr<std::ostream> StreamSession::logStream;

std::ostream& StreamSession::getLogStream () 
{
	return logStream ? *logStream : std::cout;
}

void StreamSession::SetLogFile (const std::string& filename)
{
	logStream = std::make_shared<std::ofstream> (filename, std::ofstream::out | std::ofstream::trunc);
}

void StreamSession::CloseLogFile ()
{
	logStream = nullptr;
}

//--------------------------------------------------------------------------------------------------


std::string Message::createSAMRequest(const char* format, ...)
{
    char buffer[SAM_BUFSIZE];
    memset(buffer, 0, SAM_BUFSIZE);

    va_list args;
    va_start (args, format);
    const int sizeToSend = vsnprintf(buffer, SAM_BUFSIZE, format, args);
    va_end(args);

    if (sizeToSend < 0)
    {
        print_error("Failed to format message");
        return std::string();
    }
    return std::string(buffer);
}

std::string Message::hello(const std::string &minVer, const std::string &maxVer)
{
///////////////////////////////////////////////////////////
//
//    ->  HELLO VERSION
//              MIN=$min
//              MAX=$max
//
//    <-  HELLO REPLY
//              RESULT=OK
//              VERSION=$version
//
///////////////////////////////////////////////////////////

    static const char* helloFormat = "HELLO VERSION MIN=%s MAX=%s\n";
    return createSAMRequest(helloFormat, minVer.c_str(), maxVer.c_str());
}

std::string Message::sessionCreate(SessionStyle style, const std::string& sessionID, const std::string& nickname, const std::string& destination /*= SAM_GENERATE_MY_DESTINATION*/, const std::string& options /*= ""*/)
{
///////////////////////////////////////////////////////////
//
//    ->  SESSION CREATE
//              STYLE={STREAM,DATAGRAM,RAW}
//              ID={$nickname}
//              DESTINATION={$private_destination_key,TRANSIENT}
//              [option=value]*
//
//    <-  SESSION STATUS
//              RESULT=OK
//              DESTINATION=$private_destination_key
//
///////////////////////////////////////////////////////////

    std::string sessionStyle;
    switch(style)
    {
    case sssStream:   sessionStyle = "STREAM";   break;
    case sssDatagram: sessionStyle = "DATAGRAM"; break;
    case sssRaw:      sessionStyle = "RAW";      break;
    }

    static const char* sessionCreateFormat = "SESSION CREATE STYLE=%s ID=%s DESTINATION=%s inbound.nickname=%s SIGNATURE_TYPE=7 %s\n";  // we add inbound.nickname option
    return createSAMRequest(sessionCreateFormat, sessionStyle.c_str(), sessionID.c_str(), destination.c_str(), nickname.c_str(), options.c_str());
}

std::string Message::streamAccept(const std::string& sessionID, bool silent /*= false*/)
{
///////////////////////////////////////////////////////////
//
//    ->  STREAM ACCEPT
//             ID={$nickname}
//             [SILENT={true,false}]
//
//    <-  STREAM STATUS
//             RESULT=$result
//             [MESSAGE=...]
//
///////////////////////////////////////////////////////////

    static const char* streamAcceptFormat = "STREAM ACCEPT ID=%s SILENT=%s\n";
    return createSAMRequest(streamAcceptFormat, sessionID.c_str(), silent ? "true" : "false");
}

std::string Message::streamConnect(const std::string& sessionID, const std::string& destination, bool silent /*= false*/)
{
///////////////////////////////////////////////////////////
//
//    ->  STREAM CONNECT
//             ID={$nickname}
//             DESTINATION=$peer_public_base64_key
//             [SILENT={true,false}]
//
//    <-  STREAM STATUS
//             RESULT=$result
//             [MESSAGE=...]
//
///////////////////////////////////////////////////////////

    static const char* streamConnectFormat = "STREAM CONNECT ID=%s DESTINATION=%s SILENT=%s\n";
    return createSAMRequest(streamConnectFormat, sessionID.c_str(), destination.c_str(), silent ? "true" : "false");
}

std::string Message::streamForward(const std::string& sessionID, const std::string& host, uint16_t port, bool silent /*= false*/)
{
///////////////////////////////////////////////////////////
//
//    ->  STREAM FORWARD
//             ID={$nickname}
//             PORT={$port}
//             [HOST={$host}]
//             [SILENT={true,false}]
//
//    <-  STREAM STATUS
//             RESULT=$result
//             [MESSAGE=...]
//
///////////////////////////////////////////////////////////
    static const char* streamForwardFormat = "STREAM FORWARD ID=%s PORT=%u HOST=%s SILENT=%s\n";
    return createSAMRequest(streamForwardFormat, sessionID.c_str(), (unsigned)port, host.c_str(), silent ? "true" : "false");
}

std::string Message::namingLookup(const std::string& name)
{
///////////////////////////////////////////////////////////
//
//    -> NAMING LOOKUP
//            NAME=$name
//
//    <- NAMING REPLY
//            RESULT=OK
//            NAME=$name
//            VALUE=$base64key
//
///////////////////////////////////////////////////////////

    static const char* namingLookupFormat = "NAMING LOOKUP NAME=%s\n";
    return createSAMRequest(namingLookupFormat, name.c_str());
}

std::string Message::destGenerate()
{
///////////////////////////////////////////////////////////
//
//    -> DEST GENERATE
//
//    <- DEST REPLY
//            PUB=$pubkey
//            PRIV=$privkey
//
///////////////////////////////////////////////////////////

    static const char* destGenerateFormat = "DEST GENERATE SIGNATURE_TYPE=7\n";
    return createSAMRequest(destGenerateFormat);
}

#define SAM_MAKESTRING(X) SAM_MAKESTRING2(X)
#define SAM_MAKESTRING2(X) #X

#define SAM_CHECK_RESULT(value) \
    if (result == SAM_MAKESTRING(value)) return value

Message::eStatus Message::checkAnswer(const std::string& answer)
{
    if (answer.empty())
        return EMPTY_ANSWER;

    const std::string result = getValue(answer, "RESULT");

    SAM_CHECK_RESULT(OK);
    SAM_CHECK_RESULT(DUPLICATED_DEST);
    SAM_CHECK_RESULT(DUPLICATED_ID);
    SAM_CHECK_RESULT(I2P_ERROR);
    SAM_CHECK_RESULT(INVALID_ID);
    SAM_CHECK_RESULT(INVALID_KEY);
    SAM_CHECK_RESULT(CANT_REACH_PEER);
    SAM_CHECK_RESULT(TIMEOUT);
    SAM_CHECK_RESULT(NOVERSION);
    SAM_CHECK_RESULT(KEY_NOT_FOUND);
    SAM_CHECK_RESULT(PEER_NOT_FOUND);
    SAM_CHECK_RESULT(ALREADY_ACCEPTING);

    return CANNOT_PARSE_ERROR;
}

#undef SAM_CHECK_RESULT
#undef SAM_MAKESTRING2
#undef SAM_MAKESTRING

std::string Message::getValue(const std::string& answer, const std::string& key)
{
    if (key.empty())
        return std::string();

    const std::string keyPattern = key + "=";
    size_t valueStart = answer.find(keyPattern);
    if (valueStart == std::string::npos)
        return std::string();

    valueStart += keyPattern.length();
    size_t valueEnd = answer.find_first_of(' ', valueStart);
    if (valueEnd == std::string::npos)
        valueEnd = answer.find_first_of('\n', valueStart);
    return answer.substr(valueStart, valueEnd - valueStart);
}


} // namespace SAM
