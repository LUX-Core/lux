// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2017 The lux developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "paymentserver.h"

#include "bitcoinunits.h"
#include "guiutil.h"
#include "optionsmodel.h"

#include "base58.h"
#include "chainparams.h"
#include "ui_interface.h"
#include "util.h"
#include "wallet.h"
#include "script/standard.h"

#include <cstdlib>

#include <openssl/x509_vfy.h>

#include <QApplication>
#include <QByteArray>
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileOpenEvent>
#include <QHash>
#include <QList>
#include <QLocalServer>
#include <QLocalSocket>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslError>
#include <QSslSocket>
#include <QStringList>
#include <QTextDocument>

#if QT_VERSION < 0x050000
#include <QUrl>
#else
#include <QUrlQuery>
#endif

using namespace boost;
using namespace std;

const int BITCOIN_IPC_CONNECT_TIMEOUT = 1000; // milliseconds
const QString BITCOIN_IPC_PREFIX("lux:");
// BIP70 payment protocol messages
const char* BIP70_MESSAGE_PAYMENTACK = "PaymentACK";
const char* BIP70_MESSAGE_PAYMENTREQUEST = "PaymentRequest";
// BIP71 payment protocol media types
const char* BIP71_MIMETYPE_PAYMENT = "application/lux-payment";
const char* BIP71_MIMETYPE_PAYMENTACK = "application/lux-paymentack";
const char* BIP71_MIMETYPE_PAYMENTREQUEST = "application/lux-paymentrequest";
// BIP70 max payment request size in bytes (DoS protection)
const qint64 BIP70_MAX_PAYMENTREQUEST_SIZE = 50000;

struct X509StoreDeleter {
      void operator()(X509_STORE* b) {
          X509_STORE_free(b);
      }
};

struct X509Deleter {
      void operator()(X509* b) { X509_free(b); }
};

namespace // Anon namespace
{

    std::unique_ptr<X509_STORE, X509StoreDeleter> certStore;
}

//
// Create a name that is unique for:
//  testnet / non-testnet
//  data directory
//
static QString ipcServerName()
{
    QString name("luxQt");

    // Append a simple hash of the datadir
    // Note that GetDataDir(true) returns a different path
    // for -testnet versus main net
    QString ddir(GUIUtil::boostPathToQString(GetDataDir(true)));
    name.append(QString::number(qHash(ddir)));

    return name;
}

//
// We store payment URIs and requests received before
// the main GUI window is up and ready to ask the user
// to send payment.

static QList<QString> savedPaymentRequests;

static void ReportInvalidCertificate(const QSslCertificate& cert)
{
    qDebug() << "ReportInvalidCertificate : Payment server found an invalid certificate: " << cert.subjectInfo(QSslCertificate::CommonName);
}

//
// Load OpenSSL's list of root certificate authorities
//
void PaymentServer::LoadRootCAs(X509_STORE* _store)
{
    // Unit tests mostly use this, to pass in fake root CAs:
    if (_store) {
        certStore.reset(_store);
        return;
    }

    // Normal execution, use either -rootcertificates or system certs:
    certStore.reset(X509_STORE_new());

    // Note: use "-system-" default here so that users can pass -rootcertificates=""
    // and get 'I don't like X.509 certificates, don't trust anybody' behavior:
    QString certFile = QString::fromStdString(GetArg("-rootcertificates", "-system-"));

    if (certFile.isEmpty())
        return; // Empty store

    QList<QSslCertificate> certList;

    if (certFile != "-system-") {
        certList = QSslCertificate::fromPath(certFile);
        // Use those certificates when fetching payment requests, too:
        QSslSocket::setDefaultCaCertificates(certList);
    } else
        certList = QSslSocket::systemCaCertificates();

    int nRootCerts = 0;
    const QDateTime currentTime = QDateTime::currentDateTime();
    Q_FOREACH (const QSslCertificate& cert, certList) {
        if (currentTime < cert.effectiveDate() || currentTime > cert.expiryDate()) {
            ReportInvalidCertificate(cert);
            continue;
        }
#if QT_VERSION >= 0x050000
        if (cert.isBlacklisted()) {
            ReportInvalidCertificate(cert);
            continue;
        }
#endif
        QByteArray certData = cert.toDer();
        const unsigned char* data = (const unsigned char*)certData.data();

        std::unique_ptr<X509, X509Deleter> x509(d2i_X509(0, &data, certData.size()));
        if (x509 && X509_STORE_add_cert(certStore.get(), x509.get())) {
            // Note: X509_STORE increases the reference count to the X509 object,
            // we still have to release our reference to it.
            ++nRootCerts;
        } else {
            ReportInvalidCertificate(cert);
            continue;
        }
    }
    qWarning() << "PaymentServer::LoadRootCAs : Loaded " << nRootCerts << " root certificates";

    // Project for another day:
    // Fetch certificate revocation lists, and add them to certStore.
    // Issues to consider:
    //   performance (start a thread to fetch in background?)
    //   privacy (fetch through tor/proxy so IP address isn't revealed)
    //   would it be easier to just use a compiled-in blacklist?
    //    or use Qt's blacklist?
    //   "certificate stapling" with server-side caching is more efficient
}

//
// Sending to the server is done synchronously, at startup.
// If the server isn't already running, startup continues,
// and the items in savedPaymentRequest will be handled
// when uiReady() is called.
//
// Warning: ipcSendCommandLine() is called early in init,
// so don't use "emit message()", but "QMessageBox::"!
//
void PaymentServer::ipcParseCommandLine(int argc, char* argv[])
{
    for (int i = 1; i < argc; i++) {
        QString arg(argv[i]);
        if (arg.startsWith("-"))
            continue;

        // If the lux: URI contains a payment request, we are not able to detect the
        // network as that would require fetching and parsing the payment request.
        // That means clicking such an URI which contains a testnet payment request
        // will start a mainnet instance and throw a "wrong network" error.
        if (arg.startsWith(BITCOIN_IPC_PREFIX, Qt::CaseInsensitive)) // lux: URI
        {
            savedPaymentRequests.append(arg);

            SendCoinsRecipient r;
            if (GUIUtil::parseBitcoinURI(arg, &r) && !r.address.isEmpty()) {
                auto tempChainParams = CreateChainParams(CBaseChainParams::MAIN);

                if (IsValidDestinationString(r.address.toStdString(), *tempChainParams)) {
                    SelectParams(CBaseChainParams::MAIN);
                } else {
                    tempChainParams = CreateChainParams(CBaseChainParams::TESTNET);
                    if (IsValidDestinationString(r.address.toStdString(), *tempChainParams)) {
                        SelectParams(CBaseChainParams::TESTNET);
                    } else {
                        tempChainParams = CreateChainParams(CBaseChainParams::SEGWITTEST);
                        if (IsValidDestinationString(r.address.toStdString(), *tempChainParams)) {
                            SelectParams(CBaseChainParams::SEGWITTEST);
                        }
                    }
                }
            }
        } else if (QFile::exists(arg)) // Filename
        {
            savedPaymentRequests.append(arg);

            PaymentRequestPlus request;
            if (readPaymentRequestFromFile(arg, request)) {
                if (request.getDetails().network() == "main") {
                    SelectParams(CBaseChainParams::MAIN);
                } else if (request.getDetails().network() == "test") {
                    SelectParams(CBaseChainParams::TESTNET);
                }
            }
        } else {
            // Printing to debug.log is about the best we can do here, the
            // GUI hasn't started yet so we can't pop up a message box.
            qWarning() << "PaymentServer::ipcSendCommandLine : Payment request file does not exist: " << arg;
        }
    }
}

//
// Sending to the server is done synchronously, at startup.
// If the server isn't already running, startup continues,
// and the items in savedPaymentRequest will be handled
// when uiReady() is called.
//
bool PaymentServer::ipcSendCommandLine()
{
    bool fResult = false;
    Q_FOREACH (const QString& r, savedPaymentRequests) {
        QLocalSocket* socket = new QLocalSocket();
        socket->connectToServer(ipcServerName(), QIODevice::WriteOnly);
        if (!socket->waitForConnected(BITCOIN_IPC_CONNECT_TIMEOUT)) {
            delete socket;
            socket = NULL;
            return false;
        }

        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_4_0);
        out << r;
        out.device()->seek(0);

        socket->write(block);
        socket->flush();
        socket->waitForBytesWritten(BITCOIN_IPC_CONNECT_TIMEOUT);
        socket->disconnectFromServer();

        delete socket;
        socket = NULL;
        fResult = true;
    }

    return fResult;
}

PaymentServer::PaymentServer(QObject* parent, bool startLocalServer) : QObject(parent),
                                                                       saveURIs(true),
                                                                       uriServer(0),
                                                                       netManager(0),
                                                                       optionsModel(0)
{
    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // Install global event filter to catch QFileOpenEvents
    // on Mac: sent when you click lux: links
    // other OSes: helpful when dealing with payment request files (in the future)
    if (parent)
        parent->installEventFilter(this);

    QString name = ipcServerName();

    // Clean up old socket leftover from a crash:
    QLocalServer::removeServer(name);

    if (startLocalServer) {
        uriServer = new QLocalServer(this);

        // In some case, it takes time to cleaning up old sokets leftover from a crash
        // So, we need to retry it a couple of time
        int max_tries = 20;

        while (max_tries > 0)
        {
            if (uriServer->listen(name))
            {
                connect(uriServer, SIGNAL(newConnection()), this, SLOT(handleURIConnection()));
                connect(this, SIGNAL(receivedPaymentACK(QString)), this, SLOT(handlePaymentACK(QString)));
                return;
            }

            max_tries -= 1;
            if ((max_tries % 5) == 0)
            {
                // Retry to clean up old socket leftover from a crash
                // after 5 seconds
                QLocalServer::removeServer(name);
            }

            MilliSleep(1000);
        }

        // constructor is called early in init, so don't use "emit message()" here
        QMessageBox::critical(0, tr("Payment request error"),
            tr("Cannot start lux: click-to-pay handler"));
    }
}

PaymentServer::~PaymentServer()
{
    google::protobuf::ShutdownProtobufLibrary();
}

//
// OSX-specific way of handling lux: URIs and
// PaymentRequest mime types
//
bool PaymentServer::eventFilter(QObject* object, QEvent* event)
{
    // clicking on lux: URIs creates FileOpen events on the Mac
    if (event->type() == QEvent::FileOpen) {
        QFileOpenEvent* fileEvent = static_cast<QFileOpenEvent*>(event);
        if (!fileEvent->file().isEmpty())
            handleURIOrFile(fileEvent->file());
        else if (!fileEvent->url().isEmpty())
            handleURIOrFile(fileEvent->url().toString());

        return true;
    }

    return QObject::eventFilter(object, event);
}

void PaymentServer::initNetManager()
{
    if (!optionsModel)
        return;
    if (netManager != NULL)
        delete netManager;

    // netManager is used to fetch paymentrequests given in lux: URIs
    netManager = new QNetworkAccessManager(this);

    QNetworkProxy proxy;

    // Query active SOCKS5 proxy
    if (optionsModel->getProxySettings(proxy)) {
        netManager->setProxy(proxy);

        qDebug() << "PaymentServer::initNetManager : Using SOCKS5 proxy" << proxy.hostName() << ":" << proxy.port();
    } else
        qDebug() << "PaymentServer::initNetManager : No active proxy server found.";

    connect(netManager, SIGNAL(finished(QNetworkReply*)),
        this, SLOT(netRequestFinished(QNetworkReply*)));
    connect(netManager, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError>&)),
        this, SLOT(reportSslErrors(QNetworkReply*, const QList<QSslError>&)));
}

void PaymentServer::uiReady()
{
    initNetManager();

    saveURIs = false;
    Q_FOREACH (const QString& s, savedPaymentRequests) {
        handleURIOrFile(s);
    }
    savedPaymentRequests.clear();
}

void PaymentServer::handleURIOrFile(const QString& s)
{
    if (saveURIs) {
        savedPaymentRequests.append(s);
        return;
    }

    if (s.startsWith(BITCOIN_IPC_PREFIX, Qt::CaseInsensitive)) // lux: URI
    {
#if QT_VERSION < 0x050000
        QUrl uri(s);
#else
        QUrlQuery uri((QUrl(s)));
#endif
        if (uri.hasQueryItem("r")) // payment request URI
        {
            QByteArray temp;
            temp.append(uri.queryItemValue("r"));
            QString decoded = QUrl::fromPercentEncoding(temp);
            QUrl fetchUrl(decoded, QUrl::StrictMode);

            if (fetchUrl.isValid()) {
                qDebug() << "PaymentServer::handleURIOrFile : fetchRequest(" << fetchUrl << ")";
                fetchRequest(fetchUrl);
            } else {
                qWarning() << "PaymentServer::handleURIOrFile : Invalid URL: " << fetchUrl;
                Q_EMIT message(tr("URI handling"),
                    tr("Payment request fetch URL is invalid: %1").arg(fetchUrl.toString()),
                    CClientUIInterface::ICON_WARNING);
            }

            return;
        } else // normal URI
        {
            SendCoinsRecipient recipient;
            if (GUIUtil::parseBitcoinURI(s, &recipient)) {
                CTxDestination address = DecodeDestination(recipient.address.toStdString());
                if (!IsValidDestination(address)) {
                    Q_EMIT message(tr("URI handling"), tr("Invalid payment address %1").arg(recipient.address),
                        CClientUIInterface::MSG_ERROR);
                } else
                    Q_EMIT receivedPaymentRequest(recipient);
            } else
                Q_EMIT message(tr("URI handling"),
                    tr("URI cannot be parsed! This can be caused by an invalid lux address or malformed URI parameters."),
                    CClientUIInterface::ICON_WARNING);

            return;
        }
    }

    if (QFile::exists(s)) // payment request file
    {
        PaymentRequestPlus request;
        SendCoinsRecipient recipient;
        if (!readPaymentRequestFromFile(s, request)) {
            Q_EMIT message(tr("Payment request file handling"),
                tr("Payment request file cannot be read! This can be caused by an invalid payment request file."),
                CClientUIInterface::ICON_WARNING);
        } else if (processPaymentRequest(request, recipient))
            Q_EMIT receivedPaymentRequest(recipient);

        return;
    }
}

void PaymentServer::handleURIConnection()
{
    QLocalSocket* clientConnection = uriServer->nextPendingConnection();

    while (clientConnection->bytesAvailable() < (int)sizeof(quint32))
        clientConnection->waitForReadyRead();

    connect(clientConnection, SIGNAL(disconnected()),
        clientConnection, SLOT(deleteLater()));

    QDataStream in(clientConnection);
    in.setVersion(QDataStream::Qt_4_0);
    if (clientConnection->bytesAvailable() < (int)sizeof(quint16)) {
        return;
    }
    QString msg;
    in >> msg;

    handleURIOrFile(msg);
}

//
// Warning: readPaymentRequestFromFile() is used in ipcSendCommandLine()
// so don't use "emit message()", but "QMessageBox::"!
//
bool PaymentServer::readPaymentRequestFromFile(const QString& filename, PaymentRequestPlus& request)
{
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << QString("PaymentServer::%1: Failed to open %2").arg(__func__).arg(filename);
        return false;
    }

    // BIP70 DoS protection
    if (f.size() > BIP70_MAX_PAYMENTREQUEST_SIZE) {
        qWarning() << QString("PaymentServer::%1: Payment request %2 is too large (%3 bytes, allowed %4 bytes).")
                          .arg(__func__)
                          .arg(filename)
                          .arg(f.size())
                          .arg(BIP70_MAX_PAYMENTREQUEST_SIZE);
        return false;
    }

    QByteArray data = f.readAll();

    return request.parse(data);
}

bool PaymentServer::processPaymentRequest(PaymentRequestPlus& request, SendCoinsRecipient& recipient)
{
    if (!optionsModel)
        return false;

    if (request.IsInitialized()) {
        const payments::PaymentDetails& details = request.getDetails();

        // Payment request network matches client network?
        if (details.network() != Params().NetworkIDString()) {
            Q_EMIT message(tr("Payment request rejected"), tr("Payment request network doesn't match client network."),
                CClientUIInterface::MSG_ERROR);

            return false;
        }

        // Expired payment request?
        if (details.has_expires() && (int64_t)details.expires() < GetTime()) {
            Q_EMIT message(tr("Payment request rejected"), tr("Payment request has expired."),
                CClientUIInterface::MSG_ERROR);

            return false;
        }
    } else {
        Q_EMIT message(tr("Payment request error"), tr("Payment request is not initialized."),
            CClientUIInterface::MSG_ERROR);

        return false;
    }

    recipient.paymentRequest = request;
    recipient.message = GUIUtil::HtmlEscape(request.getDetails().memo());

    request.getMerchant(certStore.get(), recipient.authenticatedMerchant);

    QList<std::pair<CScript, CAmount> > sendingTos = request.getPayTo();
    QStringList addresses;

    Q_FOREACH (const PAIRTYPE(CScript, CAmount) & sendingTo, sendingTos) {
        // Extract and check destination addresses
        CTxDestination dest;
        if (ExtractDestination(sendingTo.first, dest)) {
            // Append destination address
            addresses.append(QString::fromStdString(EncodeDestination(dest)));
        } else if (!recipient.authenticatedMerchant.isEmpty()) {
            // Insecure payments to custom lux addresses are not supported
            // (there is no good way to tell the user where they are paying in a way
            // they'd have a chance of understanding).
            Q_EMIT message(tr("Payment request rejected"),
                tr("Unverified payment requests to custom payment scripts are unsupported."),
                CClientUIInterface::MSG_ERROR);
            return false;
        }

        // Extract and check amounts
        CTxOut txOut(sendingTo.second, sendingTo.first);
        if (txOut.IsDust(::minRelayTxFee)) {
            Q_EMIT message(tr("Payment request error"), tr("Requested payment amount of %1 is too small (considered dust).").arg(BitcoinUnits::formatWithUnit(optionsModel->getDisplayUnit(), sendingTo.second)),
                CClientUIInterface::MSG_ERROR);

            return false;
        }

        recipient.amount += sendingTo.second;
    }
    // Store addresses and format them to fit nicely into the GUI
    recipient.address = addresses.join("<br />");

    if (!recipient.authenticatedMerchant.isEmpty()) {
        qDebug() << "PaymentServer::processPaymentRequest : Secure payment request from " << recipient.authenticatedMerchant;
    } else {
        qDebug() << "PaymentServer::processPaymentRequest : Insecure payment request to " << addresses.join(", ");
    }

    return true;
}

void PaymentServer::fetchRequest(const QUrl& url)
{
    QNetworkRequest netRequest;
    netRequest.setAttribute(QNetworkRequest::User, BIP70_MESSAGE_PAYMENTREQUEST);
    netRequest.setUrl(url);
    netRequest.setRawHeader("User-Agent", CLIENT_NAME.c_str());
    netRequest.setRawHeader("Accept", BIP71_MIMETYPE_PAYMENTREQUEST);
    netManager->get(netRequest);
}

void PaymentServer::fetchPaymentACK(CWallet* wallet, SendCoinsRecipient recipient, QByteArray transaction)
{
    const payments::PaymentDetails& details = recipient.paymentRequest.getDetails();
    if (!details.has_payment_url())
        return;

    QNetworkRequest netRequest;
    netRequest.setAttribute(QNetworkRequest::User, BIP70_MESSAGE_PAYMENTACK);
    netRequest.setUrl(QString::fromStdString(details.payment_url()));
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, BIP71_MIMETYPE_PAYMENT);
    netRequest.setRawHeader("User-Agent", CLIENT_NAME.c_str());
    netRequest.setRawHeader("Accept", BIP71_MIMETYPE_PAYMENTACK);

    payments::Payment payment;
    payment.set_merchant_data(details.merchant_data());
    payment.add_transactions(transaction.data(), transaction.size());

    // Create a new refund address, or re-use:
    QString account = tr("Refund from %1").arg(recipient.authenticatedMerchant);
    std::string strAccount = account.toStdString();
    set<CTxDestination> refundAddresses = wallet->GetAccountAddresses(strAccount);
    if (!refundAddresses.empty()) {
        CScript s = GetScriptForDestination(*refundAddresses.begin());
        payments::Output* refund_to = payment.add_refund_to();
        refund_to->set_script(&s[0], s.size());
    } else {
        CPubKey newKey;
        if (wallet->GetKeyFromPool(newKey, false)) {
            CKeyID keyID = newKey.GetID();
            wallet->SetAddressBook(keyID, strAccount, "refund");

            CScript s = GetScriptForDestination(keyID);
            payments::Output* refund_to = payment.add_refund_to();
            refund_to->set_script(&s[0], s.size());
        } else {
            // This should never happen, because sending coins should have
            // just unlocked the wallet and refilled the keypool.
            qWarning() << "PaymentServer::fetchPaymentACK : Error getting refund key, refund_to not set";
        }
    }

    int length = payment.ByteSize();
    netRequest.setHeader(QNetworkRequest::ContentLengthHeader, length);
    QByteArray serData(length, '\0');
    if (payment.SerializeToArray(serData.data(), length)) {
        netManager->post(netRequest, serData);
    } else {
        // This should never happen, either.
        qWarning() << "PaymentServer::fetchPaymentACK : Error serializing payment message";
    }
}

void PaymentServer::netRequestFinished(QNetworkReply* reply)
{
    reply->deleteLater();

    // BIP70 DoS protection
    if (reply->size() > BIP70_MAX_PAYMENTREQUEST_SIZE) {
        QString msg = tr("Payment request %1 is too large (%2 bytes, allowed %3 bytes).")
                          .arg(reply->request().url().toString())
                          .arg(reply->size())
                          .arg(BIP70_MAX_PAYMENTREQUEST_SIZE);

        qWarning() << QString("PaymentServer::%1:").arg(__func__) << msg;
        Q_EMIT message(tr("Payment request DoS protection"), msg, CClientUIInterface::MSG_ERROR);
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        QString msg = tr("Error communicating with %1: %2")
                          .arg(reply->request().url().toString())
                          .arg(reply->errorString());

        qWarning() << "PaymentServer::netRequestFinished: " << msg;
        Q_EMIT message(tr("Payment request error"), msg, CClientUIInterface::MSG_ERROR);
        return;
    }

    QByteArray data = reply->readAll();

    QString requestType = reply->request().attribute(QNetworkRequest::User).toString();
    if (requestType == BIP70_MESSAGE_PAYMENTREQUEST) {
        PaymentRequestPlus request;
        SendCoinsRecipient recipient;
        if (!request.parse(data)) {
            qWarning() << "PaymentServer::netRequestFinished : Error parsing payment request";
            Q_EMIT message(tr("Payment request error"),
                tr("Payment request cannot be parsed!"),
                CClientUIInterface::MSG_ERROR);
        } else if (processPaymentRequest(request, recipient))
            Q_EMIT receivedPaymentRequest(recipient);

        return;
    } else if (requestType == BIP70_MESSAGE_PAYMENTACK) {
        payments::PaymentACK paymentACK;
        if (!paymentACK.ParseFromArray(data.data(), data.size())) {
            QString msg = tr("Bad response from server %1")
                              .arg(reply->request().url().toString());

            qWarning() << "PaymentServer::netRequestFinished : " << msg;
            Q_EMIT message(tr("Payment request error"), msg, CClientUIInterface::MSG_ERROR);
        } else {
            Q_EMIT receivedPaymentACK(GUIUtil::HtmlEscape(paymentACK.memo()));
        }
    }
}

void PaymentServer::reportSslErrors(QNetworkReply* reply, const QList<QSslError>& errs)
{
    Q_UNUSED(reply);

    QString errString;
    Q_FOREACH (const QSslError& err, errs) {
        qWarning() << "PaymentServer::reportSslErrors : " << err;
        errString += err.errorString() + "\n";
    }
    Q_EMIT message(tr("Network request error"), errString, CClientUIInterface::MSG_ERROR);
}

void PaymentServer::setOptionsModel(OptionsModel* optionsModel)
{
    this->optionsModel = optionsModel;
}

void PaymentServer::handlePaymentACK(const QString& paymentACKMsg)
{
    // currently we don't futher process or store the paymentACK message
    Q_EMIT message(tr("Payment acknowledged"), paymentACKMsg, CClientUIInterface::ICON_INFORMATION | CClientUIInterface::MODAL);
}

X509_STORE* PaymentServer::getCertStore()
{
    return certStore.get();
}
