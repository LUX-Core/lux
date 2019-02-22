#include "luxupdater.h"
#include "luxupdater_p.h"
#include "../updatecontroller_p.h"

#include <QtCore/QDebug>
#include <QtCore/QList>
#include <QtCore/QMap>

using namespace QtLuxUpdater;

const QStringList LuxUpdater::NormalUpdateArguments = {QStringLiteral("--updater")};
const QStringList LuxUpdater::PassiveUpdateArguments = {QStringLiteral("--updater"), QStringLiteral("skipPrompt=true")};
const QStringList LuxUpdater::HiddenUpdateArguments = {QStringLiteral("--silentUpdate")};

LuxUpdater::LuxUpdater(QObject *parent) :
	LuxUpdater("", parent)
{}

LuxUpdater::LuxUpdater(const QString &currentVersion, QObject *parent) :
	QObject(parent),
	d(new LuxUpdaterPrivate(this))
{
	d->currentVersion = currentVersion;
}

LuxUpdater::~LuxUpdater() {}

bool LuxUpdater::exitedNormally() const
{
	return d->normalExit;
}

int LuxUpdater::errorCode() const
{
	return d->lastErrorCode;
}

QByteArray LuxUpdater::errorLog() const
{
	return d->lastErrorLog;
}

bool LuxUpdater::willRunOnExit() const
{
	return d->runOnExit;
}

QString LuxUpdater::currentVersion() const
{
	return d->currentVersion;
}

bool LuxUpdater::isRunning() const
{
	return isUpdaterRunning;
}

QList<LuxUpdater::LuxUpdateInfo> LuxUpdater::updateInfo() const
{
	return d->updateInfos;
}

bool LuxUpdater::checkForUpdates()
{
	return d->startUpdateCheck();
}

void LuxUpdater::abortUpdateCheck(int maxDelay, bool async)
{
	d->stopUpdateCheck(maxDelay, async);
}

int LuxUpdater::scheduleUpdate(int delaySeconds, bool repeated)
{
	if((((qint64)delaySeconds) * 1000ll) > (qint64)INT_MAX) {
		qCWarning(logLuxUpdater) << "delaySeconds to big to be converted to msecs";
		return 0;
	}
	return d->scheduler->startSchedule(delaySeconds * 1000, repeated);
}

int LuxUpdater::scheduleUpdate(const QDateTime &when)
{
	return d->scheduler->startSchedule(when);
}

void LuxUpdater::cancelScheduledUpdate(int taskId)
{
	d->scheduler->cancelSchedule(taskId);
}

void LuxUpdater::runUpdaterOnExit(AdminAuthoriser *authoriser)
{
	runUpdaterOnExit(NormalUpdateArguments, authoriser);
}

void LuxUpdater::runUpdaterOnExit(const QStringList &arguments, AdminAuthoriser *authoriser)
{
	d->runOnExit = true;
	d->runArguments = arguments;
	d->adminAuth.reset(authoriser);
}

void LuxUpdater::cancelExitRun()
{
	d->runOnExit = false;
	d->adminAuth.reset();
}



LuxUpdater::LuxUpdateInfo::LuxUpdateInfo() :
	name(),
	version(),
	size(0ull)
{}

LuxUpdater::LuxUpdateInfo::LuxUpdateInfo(const LuxUpdater::LuxUpdateInfo &other) :
	name(other.name),
	version(other.version),
	size(other.size)
{}

LuxUpdater::LuxUpdateInfo::LuxUpdateInfo(QString name, QString version, quint64 size) :
	name(name),
	version(version),
	size(size)
{}

QDebug &operator<<(QDebug &debug, const LuxUpdater::LuxUpdateInfo &info)
{
	QDebugStateSaver state(debug);
	Q_UNUSED(state);

	debug.noquote() << QStringLiteral("{Name: \"%1\"; Version: %2; Size: %3}")
					   .arg(info.name)
					   .arg(info.version.toUtf8().constData())
					   .arg(info.size);
	return debug;
}
