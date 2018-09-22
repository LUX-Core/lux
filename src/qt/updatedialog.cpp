#include "updatedialog.h"
#include "clientversion.h"

#include <boost/foreach.hpp>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>

#define paternVersion "lux-([0-9]+\\.)?([0-9]+\\.)?([0-9]+)-"

UpdateDialog::UpdateDialog(QObject *parent) : QObject(parent) {
    currentVersion = Version(CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR, CLIENT_VERSION_REVISION);
}

UpdateDialog::~UpdateDialog() {

}

bool UpdateDialog::newUpdateAvailable() {
    Version latestReleaseVersion = getUpdateVersion();
    return latestReleaseVersion > currentVersion;
}

QList <Version> UpdateDialog::getVersions() {
    QNetworkAccessManager manager;
    QNetworkReply *response = manager.get(QNetworkRequest(QUrl(NEW_RELEASES)));
    QEventLoop event;
    connect(response, SIGNAL(finished()), &event, SLOT(quit()));
    event.exec();
    QString html = response->readAll();
    QRegularExpression regEx(paternVersion);
    QRegularExpressionMatchIterator regExIt = regEx.globalMatch(html);
    QList <Version> versions;

    while (regExIt.hasNext()) {
        QRegularExpressionMatch match = regExIt.next();
        QString versionString = match.captured().mid(5, match.captured().length() - 6);
        Version version(versionString);
        if (!versions.contains(version)) { versions.append(version); }
    }
    return versions;
}

Version UpdateDialog::getUpdateVersion() {
    QList <Version> versions = getVersions();
    Version latestVersion;

    if (!versions.isEmpty()) {
        latestVersion = *std::max_element(versions.begin(), versions.end());
    }
    return latestVersion;
}
