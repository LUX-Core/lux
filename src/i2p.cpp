// Copyright (c) 2012-2013 giv
// Copyright (c) 2017-2018 orignal
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//--------------------------------------------------------------------------------------------------
#include <boost/thread/shared_mutex.hpp>
#include <iostream>
#include "i2p.h"
#include "utilstrencodings.h"
#include "hash.h"

namespace SAM
{

class StreamSessionAdapter::SessionHolder
{
public:
	explicit SessionHolder(std::shared_ptr<SAM::StreamSession> session);
	~SessionHolder();

	const SAM::StreamSession& getSession() const;
	SAM::StreamSession& getSession();
private:
	void heal() const;
	void reborn() const;

	mutable std::shared_ptr<SAM::StreamSession> session_;
	typedef boost::shared_mutex mutex_type;
	mutable mutex_type mtx_;
};

StreamSessionAdapter::SessionHolder::SessionHolder(std::shared_ptr<SAM::StreamSession> session)
	: session_(session)
{
}

StreamSessionAdapter::SessionHolder::~SessionHolder()
{
}

const SAM::StreamSession& StreamSessionAdapter::SessionHolder::getSession() const
{
	boost::upgrade_lock<mutex_type> lock(mtx_);
	if (session_->isSick())
	{
		boost::upgrade_to_unique_lock<mutex_type> ulock(lock);
		heal();
	}
	return *session_;
}

SAM::StreamSession& StreamSessionAdapter::SessionHolder::getSession()
{
	boost::upgrade_lock<mutex_type> lock(mtx_);
	if (session_->isSick())
	{
		boost::upgrade_to_unique_lock<mutex_type> ulock(lock);
		heal();
	}
	return *session_;
}

void StreamSessionAdapter::SessionHolder::heal() const
{
	reborn(); // if we don't know how to heal it just reborn it
}

void StreamSessionAdapter::SessionHolder::reborn() const
{
	if (!session_->isSick())
		return;
	std::shared_ptr<SAM::StreamSession> newSession(new SAM::StreamSession(*session_));
	if (!newSession->isSick() && session_->isSick())
		session_ = newSession;
}

//--------------------------------------------------------------------------------------------------

StreamSessionAdapter::StreamSessionAdapter()
{
	SAM::StreamSession::SetLogFile ((GetDataDir() / "sam.log").string ());
}

StreamSessionAdapter::~StreamSessionAdapter()
{
	SAM::StreamSession::CloseLogFile ();
}

bool StreamSessionAdapter::StartSession (
		const std::string& nickname,
		const std::string& SAMHost       /*= SAM_DEFAULT_ADDRESS*/,
		uint16_t SAMPort                 /*= SAM_DEFAULT_PORT*/,
		const std::string& myDestination /*= SAM_GENERATE_MY_DESTINATION*/,
		const std::string& i2pOptions    /*= SAM_DEFAULT_I2P_OPTIONS*/,
		const std::string& minVer        /*= SAM_DEFAULT_MIN_VER*/,
		const std::string& maxVer        /*= SAM_DEFAULT_MAX_VER*/)
{
	std::cout << "Creating SAM session..." << std::endl;
	auto s = std::make_shared<SAM::StreamSession>(nickname, SAMHost, SAMPort, myDestination, i2pOptions, minVer, maxVer);
	sessionHolder_ = std::make_shared<SessionHolder>(s);
	bool isReady = s->isReady ();
	if (isReady)
		std::cout << "SAM session created" << std::endl;
	else
		std::cout << "SAM session failed" << std::endl;
	return isReady;
}

void StreamSessionAdapter::StopSession ()
{
	std::cout << "Terminating SAM session..." << std::endl;
	sessionHolder_ = nullptr;
	std::cout << "SAM session terminated" << std::endl;
}

bool StreamSessionAdapter::Start ()
{
	return StartSession(
		GetArg(I2P_SESSION_NAME_PARAM, I2P_SESSION_NAME_DEFAULT),
		GetArg(I2P_SAM_HOST_PARAM, I2P_SAM_HOST_DEFAULT),
		(uint16_t)GetArg(I2P_SAM_PORT_PARAM, I2P_SAM_PORT_DEFAULT),
		GetArg(I2P_SAM_MY_DESTINATION_PARAM, I2P_SAM_MY_DESTINATION_DEFAULT),
		GetArg(I2P_SAM_I2P_OPTIONS_PARAM, SAM_DEFAULT_I2P_OPTIONS));
}

void StreamSessionAdapter::Stop ()
{
	StopSession ();
}

SAM::SOCKET StreamSessionAdapter::accept(bool silent)
{
	SAM::RequestResult<std::shared_ptr<SAM::Socket> > result = sessionHolder_->getSession().accept(silent);
	// call Socket::release
	return result.isOk ? result.value->release() : SAM_INVALID_SOCKET;
}

SAM::SOCKET StreamSessionAdapter::connect(const std::string& destination, bool silent)
{
	SAM::RequestResult<std::shared_ptr<SAM::Socket> > result = sessionHolder_->getSession().connect(destination, silent);
	// call Socket::release
	return result.isOk ? result.value->release() : SAM_INVALID_SOCKET;
}

bool StreamSessionAdapter::forward(const std::string& host, uint16_t port, bool silent)
{
	return sessionHolder_->getSession().forward(host, port, silent).isOk;
}

std::string StreamSessionAdapter::namingLookup(const std::string& name) const
{
	SAM::RequestResult<const std::string> result = sessionHolder_->getSession().namingLookup(name);
	return result.isOk ? result.value : std::string();
}

SAM::FullDestination StreamSessionAdapter::destGenerate() const
{
	SAM::RequestResult<const SAM::FullDestination> result = sessionHolder_->getSession().destGenerate();
	return result.isOk ? result.value : SAM::FullDestination();
}

void StreamSessionAdapter::stopForwarding(const std::string& host, uint16_t port)
{
	sessionHolder_->getSession().stopForwarding(host, port);
}

void StreamSessionAdapter::stopForwardingAll()
{
	sessionHolder_->getSession().stopForwardingAll();
}

const SAM::FullDestination& StreamSessionAdapter::getMyDestination() const
{
	return sessionHolder_->getSession().getMyDestination();
}

const sockaddr_in& StreamSessionAdapter::getSAMAddress() const
{
	return sessionHolder_->getSession().getSAMAddress();
}

const std::string& StreamSessionAdapter::getSAMHost() const
{
	return sessionHolder_->getSession().getSAMHost();
}

uint16_t StreamSessionAdapter::getSAMPort() const
{
	return sessionHolder_->getSession().getSAMPort();
}

const std::string& StreamSessionAdapter::getNickname() const
{
	return sessionHolder_->getSession().getNickname();
}

const std::string& StreamSessionAdapter::getSAMMinVer() const
{
	return sessionHolder_->getSession().getSAMMinVer();
}

const std::string& StreamSessionAdapter::getSAMMaxVer() const
{
	return sessionHolder_->getSession().getSAMMaxVer();
}

const std::string& StreamSessionAdapter::getSAMVersion() const
{
	return sessionHolder_->getSession().getSAMVersion();
}

const std::string& StreamSessionAdapter::getOptions() const
{
	return sessionHolder_->getSession().getOptions();
}

} // namespace SAM

//--------------------------------------------------------------------------------------------------

I2PSession::I2PSession()
{}

I2PSession::~I2PSession()
{}


/*static*/
std::string I2PSession::GenerateB32AddressFromDestination(const std::string& destination)
{
	std::string canonicalDest = destination;
	for (size_t pos = canonicalDest.find_first_of('-'); pos != std::string::npos; pos = canonicalDest.find_first_of('-', pos))
		canonicalDest[pos] = '+';
	for (size_t pos = canonicalDest.find_first_of('~'); pos != std::string::npos; pos = canonicalDest.find_first_of('~', pos))
		canonicalDest[pos] = '/';
	std::string rawHash = DecodeBase64(canonicalDest);
	uint256 hash;
	SHA256((const unsigned char*)rawHash.c_str(), rawHash.size(), (unsigned char*)&hash);
	std::string result = EncodeBase32(hash.begin(), hash.end() - hash.begin()) + ".b32.i2p";
	for (size_t pos = result.find_first_of('='); pos != std::string::npos; pos = result.find_first_of('=', pos-1))
		result.erase(pos, 1);
	return result;
}
