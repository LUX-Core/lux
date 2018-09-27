#ifndef UPDATEDIALOG_H
#define UPDATEDIALOG_H

#include <QObject>

#define NEW_RELEASES "https://github.com/LUX-Core/lux/releases"

class Version {

public:
    int _major;
    int _minor;
    int _revision;

    Version() { SetNull(); }

    Version(int maj, int min, int rev) {
        SetNull();
        _major = maj;
        _minor = min;
        _revision = rev;
    }

    Version(QString str) {
        SetNull();

        QStringList parts = str.split(".");

        if (!parts.isEmpty())
            _major = parts[0].toInt();
        if (parts.length() > 1)
            _minor = parts[1].toInt();
        if (parts.length() > 2)
            _revision = parts[2].toInt();
    }

    Version(const Version &v) {
        _major    = v._major;
        _minor    = v._minor;
        _revision = v._revision;
    }

    bool operator>(const Version &other) const {
        return compareAll(other) > 0;
    }

    bool operator<(const Version &other) const {
        return compareAll(other) < 0;
    }

    bool operator==(const Version &other) const {
        return compareAll(other) == 0;
    }

    void SetNull() {
        _major    = 0;
        _minor    = 0;
        _revision = 0;
    }

private:
    int compare(int first, int second) const {
        int diff = first - second;
        return diff > 0 ? 1 : diff < 0 ? -1 : 0;
    }

    int compareAll(const Version &other) const {
        return 4 * compare(_major, other._major) + 2 * compare(_minor, other._minor) + compare(_revision, other._revision);
    }
};

class UpdateDialog : public QObject {
    Q_OBJECT
public:
    explicit UpdateDialog(QObject *parent = 0);

    ~UpdateDialog();

    bool newUpdateAvailable();

private:
    QList <Version> getVersions();

    Version getUpdateVersion();

    Version currentVersion;
};

#endif // UPDATEDIALOG_H
