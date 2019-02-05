#ifndef BITMEXNETWORK_H
#define BITMEXNETWORK_H

#include <QString>
#include <QObject>
#include "luxgategui_global.h"

class QNetworkAccessManager;

class BitMexNetwork : public QObject
{
    Q_OBJECT
public:
    BitMexNetwork(QObject *parent=Q_NULLPTR);
private:
    QNetworkAccessManager * nam;
    QString apiKey;
    QString apiPrivate;
    const int apiExpiresInterval {5};

    void requestGlobalHistory();
    void readConfig();
private slots:
    void allRequests();
    void replyGlobalHistory();
signals:
    void sigGlobalHistoryData(const LuxgateHistoryData & data);
};

#endif // BITMEXNETWORK_H
