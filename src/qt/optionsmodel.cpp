// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The LUX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/lux-config.h"
#endif

#include "optionsmodel.h"

#include "bitcoinunits.h"
#include "guiutil.h"
#include "i2psam/i2psam.h"

#include "amount.h"
#include "init.h"
#include "main.h"
#include "net.h"
#include "txdb.h" // for -dbcache -nLogFile defaults

#ifdef ENABLE_WALLET
#include "masternodeconfig.h"
#include "wallet.h"
#include "walletdb.h"
#endif
//#ifdef ENABLE_I2PSAM
#include "i2pwrapper.h"
//#endif
#include <QNetworkProxy>
#include <QSettings>
#include <QStringList>

OptionsModel::OptionsModel(QObject* parent) : QAbstractListModel(parent)
{
    Init();
}

void OptionsModel::addOverriddenOption(const std::string& option)
{
    strOverriddenByCommandLine += QString::fromStdString(option) + "=" + QString::fromStdString(mapArgs[option]) + " ";
}

// Writes all missing QSettings with their default values
void OptionsModel::Init()
{
    resetSettings = false;
    QSettings settings;

    // Ensure restart flag is unset on client startup
    setRestartRequired(false);

    // These are Qt-only settings:

    // Window
    if (!settings.contains("fMinimizeToTray"))
        settings.setValue("fMinimizeToTray", false);
    fMinimizeToTray = settings.value("fMinimizeToTray").toBool();

    if (!settings.contains("fMinimizeOnClose"))
        settings.setValue("fMinimizeOnClose", false);
    fMinimizeOnClose = settings.value("fMinimizeOnClose").toBool();

    // Display
    if (!settings.contains("nDisplayUnit"))
        settings.setValue("nDisplayUnit", BitcoinUnits::LUX);
    nDisplayUnit = settings.value("nDisplayUnit").toInt();

    if (!settings.contains("strThirdPartyTxUrls"))
        settings.setValue("strThirdPartyTxUrls", "");
    strThirdPartyTxUrls = settings.value("strThirdPartyTxUrls", "").toString();

    if (!settings.contains("fCoinControlFeatures"))
        settings.setValue("fCoinControlFeatures", false);
    fCoinControlFeatures = settings.value("fCoinControlFeatures", false).toBool();

    if (!settings.contains("nDarksendRounds"))
        settings.setValue("nDarksendRounds", 2);

    if (!settings.contains("nAnonymizeLuxAmount"))
        settings.setValue("nAnonymizeLuxAmount", 1000);

    nDarksendRounds = settings.value("nDarksendRounds").toLongLong();
    nAnonymizeLuxAmount = settings.value("nAnonymizeLuxAmount").toLongLong();

    if (!settings.contains("fShowMasternodesTab"))
        settings.setValue("fShowMasternodesTab", masternodeConfig.getCount());

    if (!settings.contains("fparallelMasterNode"))
        settings.setValue("fparallelMasterNode", false);
    fparallelMasterNode = settings.value("fparallelMasterNode", false).toBool();

    // These are shared with the core or have a command-line parameter
    // and we want command-line parameters to overwrite the GUI settings.
    //
    // If setting doesn't exist create it with defaults.
    //
    // If SoftSetArg() or SoftSetBoolArg() return false we were overridden
    // by command-line and show this in the UI.

    // Main
    if (!settings.contains("nDatabaseCache"))
        settings.setValue("nDatabaseCache", (qint64)nDefaultDbCache);
    if (!SoftSetArg("-dbcache", settings.value("nDatabaseCache").toString().toStdString()))
        addOverriddenOption("-dbcache");

    if (!settings.contains("nLogFile"))
        settings.setValue("nLogFile", nLogFile);
    if (!SoftSetArg("-nlogfile", settings.value("nLogFile").toString().toStdString()))
        addOverriddenOption("-nlogfile");

    if (!settings.contains("fLogEvents"))
        settings.setValue("fLogEvents", fLogEvents);
    if (!SoftSetBoolArg("-logevents", settings.value("fLogEvents").toBool()))
        addOverriddenOption("-logevents");

    if (!settings.contains("nThreadsScriptVerif"))
        settings.setValue("nThreadsScriptVerif", DEFAULT_SCRIPTCHECK_THREADS);
    if (!SoftSetArg("-par", settings.value("nThreadsScriptVerif").toString().toStdString()))
        addOverriddenOption("-par");

// Wallet
#ifdef ENABLE_WALLET
    if (!settings.contains("bSpendZeroConfChange"))
        settings.setValue("bSpendZeroConfChange", true);
    if (!SoftSetBoolArg("-spendzeroconfchange", settings.value("bSpendZeroConfChange").toBool()))
        addOverriddenOption("-spendzeroconfchange");
    if (!settings.contains("nWalletBackups"))
        settings.setValue("nWalletBackups", 10);
#endif
    if (!settings.contains("bZeroBalanceAddressToken"))
        settings.setValue("bZeroBalanceAddressToken", true);
    if (!SoftSetBoolArg("-zerobalanceaddresstoken", settings.value("bZeroBalanceAddressToken").toBool()))
        addOverriddenOption("-zerobalanceaddresstoken");

    // Network
    if (!settings.contains("fUseUPnP"))
        settings.setValue("fUseUPnP", DEFAULT_UPNP);
    if (!SoftSetBoolArg("-upnp", settings.value("fUseUPnP").toBool()))
        addOverriddenOption("-upnp");

    if (!settings.contains("fListen"))
        settings.setValue("fListen", DEFAULT_LISTEN);
    if (!SoftSetBoolArg("-listen", settings.value("fListen").toBool()))
        addOverriddenOption("-listen");

    if (!settings.contains("fUseProxy"))
        settings.setValue("fUseProxy", false);
    if (!settings.contains("addrProxy"))
        settings.setValue("addrProxy", "127.0.0.1:9050");
    // Only try to set -proxy, if user has enabled fUseProxy
    if (settings.value("fUseProxy").toBool() && !SoftSetArg("-proxy", settings.value("addrProxy").toString().toStdString()))
        addOverriddenOption("-proxy");
    else if (!settings.value("fUseProxy").toBool() && !GetArg("-proxy", "").empty())
        addOverriddenOption("-proxy");

    // Display
    if (!settings.contains("digits"))
        settings.setValue("digits", "2");
    if (!settings.contains("theme"))
        settings.setValue("theme", "");
    if (!settings.contains("fCSSexternal"))
        settings.setValue("fCSSexternal", false);
    if (!settings.contains("language"))
        settings.setValue("language", "");
    if (!SoftSetArg("-lang", settings.value("language").toString().toStdString()))
        addOverriddenOption("-lang");

    if (settings.contains("nDarksendRounds"))
        SoftSetArg("-darksendrounds", settings.value("nDarksendRounds").toString().toStdString());
    if (settings.contains("nAnonymizeLuxAmount"))
        SoftSetArg("-anonymizeluxamount", settings.value("nAnonymizeLuxAmount").toString().toStdString());

    language = settings.value("language").toString();

/*#ifdef ENABLE_I2PSAM
    eI2PUseI2POnly = IsI2POnly();
    eI2PSAMHost = QString::fromStdString( GetArg( "-i2p.options.samhost", SAM_DEFAULT_ADDRESS ) );
    eI2PSAMPort = (int)GetArg( "-i2p.options.samport", SAM_DEFAULT_PORT );
    eI2PSessionName = QString::fromStdString( GetArg( "-i2p.options.sessionname", I2P_SESSION_NAME_DEFAULT ) );

    I2PInboundQuantity        = (int)GetArg( "-i2p.options.inbound.quantity"        , SAM_DEFAULT_INBOUND_QUANTITY );
    I2PInboundLength          = (int)GetArg( "-i2p.options.inbound.length"          , SAM_DEFAULT_INBOUND_LENGTH );
    I2PInboundLengthVariance  = (int)GetArg( "-i2p.options.inbound.lengthvariance"  , SAM_DEFAULT_INBOUND_LENGTHVARIANCE );
    I2PInboundBackupQuantity  = (int)GetArg( "-i2p.options.inbound.backupquantity"  , SAM_DEFAULT_INBOUND_BACKUPQUANTITY );
    I2PInboundAllowZeroHop    = GetBoolArg(  "-i2p.options.inbound.allowzerohop"     , SAM_DEFAULT_INBOUND_ALLOWZEROHOP );
    I2PInboundIPRestriction   = (int)GetArg( "-i2p.options.inbound.iprestriction"   , SAM_DEFAULT_INBOUND_IPRESTRICTION );
    I2POutboundQuantity       = (int)GetArg( "-i2p.options.outbound.quantity"       , SAM_DEFAULT_OUTBOUND_QUANTITY );
    I2POutboundLength         = (int)GetArg( "-i2p.options.outbound.length"         , SAM_DEFAULT_OUTBOUND_LENGTH );
    I2POutboundLengthVariance = (int)GetArg( "-i2p.options.outbound.lengthvariance" , SAM_DEFAULT_OUTBOUND_LENGTHVARIANCE );
    I2POutboundBackupQuantity = (int)GetArg( "-i2p.options.outbound.backupquantity" , SAM_DEFAULT_OUTBOUND_BACKUPQUANTITY );
    I2POutboundAllowZeroHop   = GetBoolArg(  "-i2p.options.outbound.allowzerohop"    , SAM_DEFAULT_OUTBOUND_ALLOWZEROHOP );
    I2POutboundIPRestriction  = (int)GetArg( "-i2p.options.outbound.iprestriction"  , SAM_DEFAULT_OUTBOUND_IPRESTRICTION );
    I2POutboundPriority       = (int)GetArg( "-i2p.options.outbound.priority"       , SAM_DEFAULT_OUTBOUND_PRIORITY );
#endif*/
}

void OptionsModel::Reset()
{
    QSettings settings;

    // Remove all entries from our QSettings object
    settings.clear();
    resetSettings = true; // Needed in lux.cpp during shotdown to also remove the window positions

    // default setting for OptionsModel::StartAtStartup - disabled
    if (GUIUtil::GetStartOnSystemStartup())
        GUIUtil::SetStartOnSystemStartup(false);
}

int OptionsModel::rowCount(const QModelIndex& parent) const
{
    return OptionIDRowCount;
}

// read QSettings values and return them
QVariant OptionsModel::data(const QModelIndex& index, int role) const
{
    if (role == Qt::EditRole) {
        QSettings settings;
        switch (index.row()) {
        case StartAtStartup:
            return GUIUtil::GetStartOnSystemStartup();
        case MinimizeToTray:
            return fMinimizeToTray;
        case MapPortUPnP:
#ifdef USE_UPNP
            return settings.value("fUseUPnP");
#else
            return false;
#endif
        case MinimizeOnClose:
            return fMinimizeOnClose;

        // default proxy
        case ProxyUse:
            return settings.value("fUseProxy", false);
        case ProxyIP: {
            // contains IP at index 0 and port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            return strlIpPort.at(0);
        }
        case ProxyPort: {
            // contains IP at index 0 and port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            return strlIpPort.at(1);
        }

#ifdef ENABLE_WALLET
        case SpendZeroConfChange:
            return settings.value("bSpendZeroConfChange");
        case ShowMasternodesTab:
            return settings.value("fShowMasternodesTab");
#endif
        case ZeroBalanceAddressToken:
            return settings.value("bZeroBalanceAddressToken");
        case DisplayUnit:
            return nDisplayUnit;
        case ThirdPartyTxUrls:
            return strThirdPartyTxUrls;
        case Digits:
            return settings.value("digits");
        case Theme:
            return settings.value("theme");
        case Language:
            return settings.value("language");
        case CoinControlFeatures:
            return fCoinControlFeatures;
        case showMasternodesTab:
             return fshowMasternodesTab;
         case parallelMasterNode:
              return fparallelMasterNode;
        case WalletBackups:
            return QVariant(nWalletBackups);
        case DatabaseCache:
            return settings.value("nDatabaseCache");
        case LogFileCount:
            return settings.value("nLogFile");
        case LogEvents:
            return settings.value("fLogEvents");
        case ThreadsScriptVerif:
            return settings.value("nThreadsScriptVerif");
        case DarksendRounds:
            return QVariant(nDarksendRounds);
        case AnonymizeLuxAmount:
            return QVariant(nAnonymizeLuxAmount);
        case Listen:
            return settings.value("fListen");
        default:
            return QVariant();
        }
    }
    return QVariant();
}

// write QSettings values
bool OptionsModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    bool successful = true; /* set to false on parse error */
    if (role == Qt::EditRole) {
        QSettings settings;
        switch (index.row()) {
        case StartAtStartup:
            successful = GUIUtil::SetStartOnSystemStartup(value.toBool());
            break;
        case MinimizeToTray:
            fMinimizeToTray = value.toBool();
            settings.setValue("fMinimizeToTray", fMinimizeToTray);
            break;
        case MapPortUPnP: // core option - can be changed on-the-fly
            settings.setValue("fUseUPnP", value.toBool());
            MapPort(value.toBool());
            break;
        case MinimizeOnClose:
            fMinimizeOnClose = value.toBool();
            settings.setValue("fMinimizeOnClose", fMinimizeOnClose);
            break;

        // default proxy
        case ProxyUse:
            if (settings.value("fUseProxy") != value) {
                settings.setValue("fUseProxy", value.toBool());
                setRestartRequired(true);
            }
            break;
        case ProxyIP: {
            // contains current IP at index 0 and current port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            // if that key doesn't exist or has a changed IP
            if (!settings.contains("addrProxy") || strlIpPort.at(0) != value.toString()) {
                // construct new value from new IP and current port
                QString strNewValue = value.toString() + ":" + strlIpPort.at(1);
                settings.setValue("addrProxy", strNewValue);
                setRestartRequired(true);
            }
        } break;
        case ProxyPort: {
            // contains current IP at index 0 and current port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            // if that key doesn't exist or has a changed port
            if (!settings.contains("addrProxy") || strlIpPort.at(1) != value.toString()) {
                // construct new value from current IP and new port
                QString strNewValue = strlIpPort.at(0) + ":" + value.toString();
                settings.setValue("addrProxy", strNewValue);
                setRestartRequired(true);
            }
        } break;
#ifdef ENABLE_WALLET
        case SpendZeroConfChange:
            if (settings.value("bSpendZeroConfChange") != value) {
                settings.setValue("bSpendZeroConfChange", value);
                setRestartRequired(true);
            }
            break;
        case ShowMasternodesTab:
            if (settings.value("fShowMasternodesTab") != value) {
                settings.setValue("fShowMasternodesTab", value);
                setRestartRequired(true);
            }
            break;
#endif
        case ZeroBalanceAddressToken:
            bZeroBalanceAddressToken = value.toBool();
            settings.setValue("bZeroBalanceAddressToken", bZeroBalanceAddressToken);
            Q_EMIT zeroBalanceAddressTokenChanged(bZeroBalanceAddressToken);
            break;
        case DisplayUnit:
            setDisplayUnit(value);
            break;
        case ThirdPartyTxUrls:
            if (strThirdPartyTxUrls != value.toString()) {
                strThirdPartyTxUrls = value.toString();
                settings.setValue("strThirdPartyTxUrls", strThirdPartyTxUrls);
                setRestartRequired(true);
            }
            break;
        case Digits:
            if (settings.value("digits") != value) {
                settings.setValue("digits", value);
                setRestartRequired(true);
            }
            break;
        case Theme:
            if (settings.value("theme") != value) {
                settings.setValue("theme", value);
                setRestartRequired(true);
            }
            break;
        case Language:
            if (settings.value("language") != value) {
                settings.setValue("language", value);
                setRestartRequired(true);
            }
            break;
        case DarksendRounds:
            nDarksendRounds = value.toInt();
            settings.setValue("nDarksendRounds", nDarksendRounds);
            emit darksendRoundsChanged(nDarksendRounds);
            break;
        case AnonymizeLuxAmount:
            nAnonymizeLuxAmount = value.toInt();
            settings.setValue("nAnonymizeLuxAmount", nAnonymizeLuxAmount);
            emit anonymizeLuxAmountChanged(nAnonymizeLuxAmount);
            break;
        case CoinControlFeatures:
            fCoinControlFeatures = value.toBool();
            settings.setValue("fCoinControlFeatures", fCoinControlFeatures);
            emit coinControlFeaturesChanged(fCoinControlFeatures);
            break;
        case showMasternodesTab:
            fshowMasternodesTab = value.toBool();
            settings.setValue("fshowMasternodesTab", fshowMasternodesTab);
            emit showMasternodesTabChanged(fshowMasternodesTab);
             break;
         case parallelMasterNode:
             fparallelMasterNode = value.toBool();
             settings.setValue("fparallelMasterNode", fparallelMasterNode);
             emit parallelMasterNodeChanged(fparallelMasterNode);
             break;
        case WalletBackups:
            nWalletBackups = value.toInt();
            settings.setValue("nWalletBackups", nWalletBackups);
            WriteConfigToFile("createwalletbackups", std::to_string(nWalletBackups));
            emit walletBackupsChanged(nWalletBackups);
            break;
        case DatabaseCache:
            if (settings.value("nDatabaseCache") != value) {
                settings.setValue("nDatabaseCache", value);
                setRestartRequired(true);
            }
            break;
        case LogFileCount:
            if (settings.value("nLogFile") != value) {
                settings.setValue("nLogFile", value);

                //Delete existing debug log files before restart
                boost::filesystem::path pathDebug = GetDataDir() / "debug.log";
                std::string pathDebugStr = pathDebug.string();
                //remove(pathDebugStr.c_str());
                int debugNum = 1;
                while (true) {
                    std::string tempPath = pathDebugStr + ".";
                    if (debugNum < 10)
                        tempPath += "0";
                    tempPath += std::to_string(debugNum);
                    if (access( tempPath.c_str(), F_OK ) != -1) {
                        remove(tempPath.c_str());
                        debugNum++;
                    }
                    else {
                        break;
                    }
                }
                setRestartRequired(false);
            }
            break;
        case LogEvents:
            if (settings.value("fLogEvents") != value) {
               settings.setValue("fLogEvents", value);
               setRestartRequired(true);
            }
           break;
        case ThreadsScriptVerif:
            if (settings.value("nThreadsScriptVerif") != value) {
                settings.setValue("nThreadsScriptVerif", value);
                setRestartRequired(true);
            }
            break;
        case Listen:
            if (settings.value("fListen") != value) {
                settings.setValue("fListen", value);
                setRestartRequired(true);
            }
            break;
        default:
            break;
        }
    }

    emit dataChanged(index, index);

    return successful;
}

/** Updates current unit in memory, settings and emits displayUnitChanged(newUnit) signal */
void OptionsModel::setDisplayUnit(const QVariant& value)
{
    if (!value.isNull()) {
        QSettings settings;
        nDisplayUnit = value.toInt();
        settings.setValue("nDisplayUnit", nDisplayUnit);
        emit displayUnitChanged(nDisplayUnit);
    }
}

bool OptionsModel::getProxySettings(QNetworkProxy& proxy) const
{
    // Directly query current base proxy, because
    // GUI settings can be overridden with -proxy.
    proxyType curProxy;
    if (GetProxy(NET_IPV4, curProxy)) {
        proxy.setType(QNetworkProxy::Socks5Proxy);
        proxy.setHostName(QString::fromStdString(curProxy.ToStringIP()));
        proxy.setPort(curProxy.GetPort());

        return true;
    } else
        proxy.setType(QNetworkProxy::NoProxy);

    return false;
}

void OptionsModel::setRestartRequired(bool fRequired)
{
    QSettings settings;
    return settings.setValue("fRestartRequired", fRequired);
}

bool OptionsModel::isRestartRequired()
{
    QSettings settings;
    return settings.value("fRestartRequired", false).toBool();
}
