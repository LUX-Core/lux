#include "downloadmanager.h"

#include <QTextStream>

#include <cstdio>

using namespace QtLuxUpdater;

using namespace std;

DownloadManager::DownloadRequest::DownloadRequest() :
	url(),
	checkSize(),
	needToRedirect(false)
{}

DownloadManager::DownloadRequest::DownloadRequest(const DownloadManager::DownloadRequest &other) :
	url(other.url),
	checkSize(other.checkSize),
	needToRedirect(false)
{}

DownloadManager::DownloadRequest::DownloadRequest(QUrl url, bool checkSize) :
	url(url),
	checkSize(checkSize),
	needToRedirect(false)
{}


DownloadManager::DownloadProgress::DownloadProgress() :
	name(),
	receivedSize(0),
	totalSize(0),
	error("")
{}

DownloadManager::DownloadProgress::DownloadProgress(const DownloadManager::DownloadProgress &other) :
	name(other.name),
	receivedSize(other.receivedSize),
	totalSize(other.totalSize),
	error(other.error)
{}

DownloadManager::DownloadProgress::DownloadProgress(QString name, qint64 receivedSize, qint64 totalSize) :
	name(name),
	receivedSize(receivedSize),
	totalSize(totalSize),
	error("")
{}

DownloadManager::DownloadProgress::DownloadProgress(QString name, QString error) :
	name(name),
	receivedSize(0),
	totalSize(0),
	error(error)
{}


DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent)
{
}

void DownloadManager::append(const QStringList &urls, bool checkSize)
{
    for (const QString &urlAsString : urls)
        append(QUrl::fromEncoded(urlAsString.toLocal8Bit()), checkSize);

    if (downloadQueue.isEmpty())
        QTimer::singleShot(0, this, SIGNAL(finished()));
}

void DownloadManager::append(const QUrl &url, bool checkSize)
{
    if (downloadQueue.isEmpty())
        QTimer::singleShot(0, this, SLOT(startNextDownload()));

    DownloadManager::DownloadRequest req(url, checkSize);
    downloadQueue.enqueue(req);
    ++totalCount;
}

QString DownloadManager::saveFileName(const QUrl &url)
{
    QString path = url.path();
    QString basename = QFileInfo(path).fileName();

    if (basename.isEmpty())
        basename = "download";

    if (QFile::exists(basename)) {
        // already exists, don't overwrite
        int i = 0;
        basename += '.';
        while (QFile::exists(basename + QString::number(i)))
            ++i;

        basename += QString::number(i);
    }

    return basename;
}

void DownloadManager::startNextDownload()
{
    if (!currentRequest.needToRedirect) {
        if (downloadQueue.isEmpty()) {
            emit finished();
            return;
        }

        currentRequest = downloadQueue.dequeue();
    }
    currentRequest.isRedirecting = currentRequest.needToRedirect;
    currentRequest.needToRedirect = false;

    if (currentRequest.checkSize) { // get only file size
        QNetworkRequest request(currentRequest.url);
        if (currentRequest.isRedirecting) {
            currentDownload = manager.get(request);
            connect(currentDownload, SIGNAL(downloadProgress(qint64,qint64)),
                SLOT(onDownloadProgress(qint64,qint64)));
        }
        else
            currentDownload = manager.head(request);
        connect(currentDownload,SIGNAL(finished()),SLOT(onDownloadFinished()));
    } else {
        QString filename = saveFileName(currentRequest.url);
        output.setFileName(filename);
        if (!output.open(QIODevice::WriteOnly)) {

            DownloadManager::DownloadProgress progress("Lux Wallet", "Could not create file for download!");
            emit downloadFinished(progress);

            startNextDownload();
            return;                 // skip this download
        }

        QNetworkRequest request(currentRequest.url);
        currentDownload = manager.get(request);
        connect(currentDownload, SIGNAL(downloadProgress(qint64,qint64)),
                SLOT(onDownloadProgress(qint64,qint64)));
        connect(currentDownload, SIGNAL(finished()),
                SLOT(onDownloadFinished()));
        connect(currentDownload, SIGNAL(readyRead()),
                SLOT(onDownloadReadyRead()));
    }

    downloadTime.start();
}

void DownloadManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (currentRequest.isRedirecting) {
        if (currentDownload)
            currentDownload->abort();
        return;
    }
    DownloadManager::DownloadProgress progress("Lux Wallet", bytesReceived, bytesTotal);
    emit downloadProgress(progress);
}

void DownloadManager::onDownloadFinished()
{
    if (!currentRequest.checkSize) {
        output.close();

        if (currentDownload->error()) {
            output.remove();
            DownloadManager::DownloadProgress progress("Lux Wallet", currentDownload->errorString());
            emit downloadFinished(progress);
        } else {
            // let's check if it was actually a redirect
            if (isHttpRedirect()) {
                redirect();
                output.remove();
            } else {
                DownloadManager::DownloadProgress progress("Lux Wallet", 0, 0);
                emit downloadFinished(progress);
                ++downloadedCount;
            }
        }
    } else {
        if (currentDownload->error() && currentDownload->error() != QNetworkReply::OperationCanceledError) {
            DownloadManager::DownloadProgress progress("Lux Wallet", -1, -1);
            emit downloadFinished(progress);
        } else {
            // let's check if it was actually a redirect
            if (isHttpRedirect()) {
                redirect();
            } else {
                qint64 totalSize = currentDownload->header(QNetworkRequest::ContentLengthHeader).toInt();
                DownloadManager::DownloadProgress progress("Lux Wallet", -1, totalSize);
                emit downloadFinished(progress);
            }
        }
    }

    currentDownload->deleteLater();
    startNextDownload();
}

void DownloadManager::onDownloadReadyRead()
{
    output.write(currentDownload->readAll());
}

bool DownloadManager::isHttpRedirect() const
{
    int statusCode = currentDownload->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    return statusCode == 301 || statusCode == 302 || statusCode == 303
           || statusCode == 305 || statusCode == 307 || statusCode == 308;
}

void DownloadManager::redirect()
{
    int statusCode = currentDownload->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QUrl requestUrl = currentDownload->request().url();
    QTextStream(stderr) << "Request: " << requestUrl.toDisplayString()
                        << " was redirected with code: " << statusCode
                        << '\n';

    QVariant target = currentDownload->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (!target.isValid())
        return;
    QUrl redirectUrl = target.toUrl();
    if (redirectUrl.isRelative())
        redirectUrl = requestUrl.resolved(redirectUrl);
    QTextStream(stderr) << "Redirected to: " << redirectUrl.toString()
                        << '\n';
    currentRequest.url = redirectUrl;
    currentRequest.needToRedirect = true;
}
