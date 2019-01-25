#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QtNetwork>
#include <QtCore>

namespace QtLuxUpdater
{
    
class DownloadManager: public QObject
{
    Q_OBJECT
public:
    explicit DownloadManager(QObject *parent = nullptr);

    void append(const QUrl &url, bool checkSize);
    void append(const QStringList &urls, bool checkSize);
    static QString saveFileName(const QUrl &url);

public:
  	struct DownloadRequest
	{
		QUrl url;
        bool checkSize;
        bool needToRedirect;
        bool isRedirecting;

		//! Default Constructor
		DownloadRequest();
		//! Copy Constructor
		DownloadRequest(const DownloadRequest &other);
		//! Constructor that takes url and checkSize
		DownloadRequest(QUrl url, bool checkSize);
	};

    //! Provides information about download progress
	struct DownloadProgress
	{
		//! The name of the download item that is in progress
		QString name;
		//! The received size
		qint64 receivedSize;
		//! The total size
		qint64 totalSize;
        //! Message if error
        QString error;

		//! Default Constructor
		DownloadProgress();
		//! Copy Constructor
		DownloadProgress(const DownloadProgress &other);
		//! Constructor that takes name, receivedSize and totalSize
		DownloadProgress(QString name, qint64 receivedSize, qint64 totalSize);
        //! Constructor that takes name and error
        DownloadProgress(QString name, QString error);
	};

private slots:
    void startNextDownload();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();
    void onDownloadReadyRead();

Q_SIGNALS:
    void downloadProgress(DownloadManager::DownloadProgress progress);
    void downloadFinished(DownloadManager::DownloadProgress progress);
    void finished();

private:
    bool isHttpRedirect() const;
    void redirect();

    QNetworkAccessManager manager;
    QQueue<DownloadManager::DownloadRequest> downloadQueue;
    QNetworkReply *currentDownload = nullptr;
    DownloadManager::DownloadRequest currentRequest;
    QFile output;
    QTime downloadTime;

    int downloadedCount = 0;
    int totalCount = 0;
};

}
#endif
