#ifndef LUXUPDATER_ATOMFEEDER_H
#define LUXUPDATER_ATOMFEEDER_H

#include <QtCore/QObject>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QUrl>
#include <QtCore/QString>
#include <QtCore/QXmlStreamReader>

namespace QtLuxUpdater {

class AtomFeeder : public QObject
{
	Q_OBJECT

public:
	AtomFeeder(const QString &url, QObject *parent = nullptr);
	~AtomFeeder();

	void start();
	void stop();

	// get version list from atom
	const QList< QMap<QString,QString> > &getVersionList();
	int getVersionListCount();
	QString getVersion(int position);

private slots:
	void onResult(QNetworkReply* reply);

Q_SIGNALS:
	//! Will be emitted as soon as the get version list finished checking for updates
	void getVersionListDone();

private:

	QString m_url;
	QNetworkAccessManager m_networkManager;
	QNetworkReply* m_currentReply;
	QList< QMap<QString,QString> > entries;

	void getXmlFeed();
	QMap<QString, QString> parseEntry(QXmlStreamReader& xml);
	void addElementDataToMap(QXmlStreamReader& xml,
                             QMap<QString, QString>& map) const;
};

}

#endif // LUXUPDATER_ATOMFEEDER_H
