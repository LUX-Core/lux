// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_OPTIONSMODEL_H
#define BITCOIN_QT_OPTIONSMODEL_H

#include "amount.h"

#include <QAbstractListModel>

QT_BEGIN_NAMESPACE
class QNetworkProxy;
QT_END_NAMESPACE

/** Interface from Qt to configuration data structure for Bitcoin client.
   To Qt, the options are presented as a list with the different options
   laid out vertically.
   This can be changed to a tree once the settings become sufficiently
   complex.
 */
class OptionsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit OptionsModel(QObject* parent = 0);

    enum OptionID {
        StartAtStartup,          // bool
        HideTrayIcon,            // bool
        MinimizeToTray,          // bool
        MapPortUPnP,             // bool
        MinimizeOnClose,         // bool
        ProxyUse,                // bool
        ProxyIP,                 // QString
        ProxyPort,               // int
        DisplayUnit,             // BitcoinUnits::Unit
        ThirdPartyTxUrls,        // QString
        Digits,                  // QString
        Theme,                   // QString
        Language,                // QString
        CoinControlFeatures,     // bool
        ShowMasternodesTab,      // bool
        ParallelMasternodes,     // bool
        ThreadsScriptVerif,      // int
        DatabaseCache,           // int
        LogFileCount,            // int
        LogEvents,               // bool
        ZeroBalanceAddressToken, // bool
        SpendZeroConfChange,     // bool
        ShowAdvancedUI,          // bool
        DarkSendRounds,          // int
        AnonymizeLuxAmount,      // int
        NotUseChangeAddress,     // bool
        WalletBackups,           // int
        Listen,                  // bool
        CheckUpdates,            // bool
        TxIndex,                 // bool
        AddressIndex,            // bool
        OptionIDRowCount,
    };

    void Init();
    void Reset();

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);
    /** Updates current unit in memory, settings and emits displayUnitChanged(newUnit) signal */
    void setDisplayUnit(const QVariant& value);

    /* Explicit getters */
    bool getHideTrayIcon() { return fHideTrayIcon; }
    bool getMinimizeToTray() { return fMinimizeToTray; }
    bool getMinimizeOnClose() { return fMinimizeOnClose; }
    int getDisplayUnit() { return nDisplayUnit; }
    QString getThirdPartyTxUrls() { return strThirdPartyTxUrls; }
    bool getProxySettings(QNetworkProxy& proxy) const;
    bool getCoinControlFeatures() { return fCoinControlFeatures; }
    bool getShowAdvancedUI() { return fShowAdvancedUI; }
    bool getShowMasternodesTab() { return fShowMasternodesTab; }
    bool getParallelMasternodes() { return fParallelMasternodes; }
    const QString& getOverriddenByCommandLine() { return strOverriddenByCommandLine; }

    /* Restart flag helper */
    void setRestartRequired(bool fRequired);
    bool isRestartRequired();
    bool resetSettings;

private:
    /* Qt-only settings */
    bool fHideTrayIcon;
    bool fMinimizeToTray;
    bool fMinimizeOnClose;
    QString language;
    int nDisplayUnit;
    QString strThirdPartyTxUrls;
    bool fCoinControlFeatures;
    bool fShowAdvancedUI;
    bool fShowMasternodesTab;
    bool fParallelMasternodes;
    /* settings that were overriden by command-line */
    QString strOverriddenByCommandLine;

    /// Add option to list of GUI options overridden through command line/config file
    void addOverriddenOption(const std::string& option);

Q_SIGNALS:
    void displayUnitChanged(int unit);
    void darksendRoundsChanged(int);
    void darkSentAmountChanged();
    void advancedUIChanged(bool);
    void anonymizeLuxAmountChanged(int);
    void coinControlFeaturesChanged(bool);
    void showMasternodesTabChanged(bool);
    void parallelMasternodesChanged(bool);
    void zeroBalanceAddressTokenChanged(bool);
    void walletBackupsChanged(int);
    void hideTrayIconChanged(bool);
};

#endif // BITCOIN_QT_OPTIONSMODEL_H
