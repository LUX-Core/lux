#include "luxupdater.h"
#include "luxupdater_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QTimer>

#define REPOSITORY_URL "https://github.com/LUX-Core/lux/releases.atom"

std::atomic<bool> isUpdaterRunning(false);

using namespace QtLuxUpdater;

Q_LOGGING_CATEGORY(logLuxUpdater, "LuxUpdater")

LuxUpdaterPrivate::LuxUpdaterPrivate(LuxUpdater *q_ptr) :
	QObject(nullptr),
	q(q_ptr),
	currentVersion(),
	updateInfos(),
	normalExit(true),
	lastErrorCode(0),
	lastErrorLog(),
	atomFeeder(nullptr),
	currentVersionPos(-1),
	downloadManager(nullptr),
	scheduler(new SimpleScheduler(this)),
	runOnExit(false),
	runArguments(),
	adminAuth(nullptr)
{
	isUpdaterRunning = false;
	connect(qApp, &QCoreApplication::aboutToQuit,
			this, &LuxUpdaterPrivate::onAppAboutToExit,
			Qt::DirectConnection);
	connect(scheduler, &SimpleScheduler::scheduleTriggered,
			this, &LuxUpdaterPrivate::startUpdateCheck);
}

LuxUpdaterPrivate::~LuxUpdaterPrivate()
{
	if (runOnExit)
		qCWarning(logLuxUpdater) << "LuxUpdater destroyed with run on exit active before the application quit";

	if (atomFeeder) {
		delete atomFeeder;
		atomFeeder = nullptr;
	}

	if (downloadManager) {
		delete downloadManager;
		downloadManager = nullptr;
	}
}

bool LuxUpdaterPrivate::startUpdateCheck()
{
	if (isUpdaterRunning) {
		return false;
	}

	updateInfos.clear();
	normalExit = true;
	lastErrorCode = 0;
	lastErrorLog.clear();
	currentVersionPos = -1;

	atomFeeder = new AtomFeeder(REPOSITORY_URL);

	connect(atomFeeder, &AtomFeeder::getVersionListDone,
			this, &LuxUpdaterPrivate::onUpdaterReady);

	atomFeeder->start();

	isUpdaterRunning = true;

	emit q->updateInfoChanged(updateInfos);
	emit q->runningChanged(true);
	return true;
}

void LuxUpdaterPrivate::stopUpdateCheck(int delay, bool async)
{
	if (atomFeeder) {
		if (delay > 0) {
			atomFeeder->stop();
			if (async){
				QTimer::singleShot(delay, this, [this](){
					stopUpdateCheck(0, false);
				});
			} else {
				atomFeeder->stop();
			}
		} else {
			atomFeeder->stop();
		}
	}
}

QString LuxUpdaterPrivate::getDownloadUrl(QString version)
{
#if defined(Q_OS_WIN32)
	//QString fileName = "lux-qt-win32.zip";
	QString fileName = "lux-qt-win.zip";
#elif defined(Q_OS_WIN)
	//QString fileName = "lux-qt-win64.zip";
	QString fileName = "lux-qt-win.zip";
#elif defined(Q_OS_OSX)
	QString fileName = "lux-qt-mac.dmg";
#else
	QString fileName = 1 ? "lux-qt-linux-16.zip" : "lux-qt-linux-18.zip";
#endif
	return "https://github.com/LUX-Core/lux/releases/download/" + version + "/" + fileName;
}

void LuxUpdaterPrivate::onDownloadProgress(DownloadManager::DownloadProgress progress)
{

}

void LuxUpdaterPrivate::onDownloadFinished(DownloadManager::DownloadProgress progress)
{

}

void LuxUpdaterPrivate::onDownloadCheckSize(DownloadManager::DownloadProgress progress)
{
	if (progress.totalSize > 0) {
		LuxUpdater::LuxUpdateInfo updateInfo("Lux wallet", newVersion, progress.totalSize);
		updateInfos.append(updateInfo);

		if (downloadManager) {
			downloadManager->deleteLater();
			downloadManager = nullptr;
		}
		if (atomFeeder) {
			atomFeeder->deleteLater();
			atomFeeder = nullptr;
		}
		emit q->runningChanged(false);
		if (!updateInfos.isEmpty())
			emit q->updateInfoChanged(updateInfos);
		emit q->checkUpdatesDone(!updateInfos.isEmpty(), false);

		isUpdaterRunning = false;
	} else {
		onUpdaterReady();
	}
}

void LuxUpdaterPrivate::onUpdaterReady()
{
	if (atomFeeder) {
		normalExit = true;
		lastErrorCode = 0;
		lastErrorLog = "";
		currentVersionPos++;
		if (currentVersionPos < atomFeeder->getVersionListCount()) {
			QString version = atomFeeder->getVersion(currentVersionPos);
			//if (version != currentVersion)
			{
				if (downloadManager == nullptr) {
					downloadManager = new DownloadManager(this);
					connect(downloadManager, &DownloadManager::downloadFinished,
							this, &LuxUpdaterPrivate::onDownloadCheckSize);
				}
				newVersion = version;
				downloadManager->append(getDownloadUrl(version), true);
				return;
			}
		}
		if (downloadManager) {
			downloadManager->deleteLater();
			downloadManager = nullptr;
		}
		atomFeeder->deleteLater();
		atomFeeder = nullptr;
		emit q->runningChanged(false);
		if (!updateInfos.isEmpty())
			emit q->updateInfoChanged(updateInfos);
		emit q->checkUpdatesDone(!updateInfos.isEmpty(), false);

		isUpdaterRunning = false;
	}
}

void LuxUpdaterPrivate::updaterError()
{
	if (atomFeeder) {
		normalExit = false;
		lastErrorCode = -1;
		lastErrorLog = "Error!";
		atomFeeder->deleteLater();
		atomFeeder = nullptr;

		isUpdaterRunning = false;
		emit q->runningChanged(false);
		emit q->checkUpdatesDone(false, true);
	}
}

void LuxUpdaterPrivate::onAppAboutToExit()
{
	if (runOnExit) {
		QFileInfo appInfo(QCoreApplication::applicationFilePath());
		auto ok = false;
		if (adminAuth && !adminAuth->hasAdminRights())
			ok = adminAuth->executeAsAdmin(appInfo.absoluteFilePath(), runArguments);
		else {
			ok = QProcess::startDetached(appInfo.absoluteFilePath(),
										 runArguments,
										 appInfo.absolutePath());
		}

		if (!ok) {
			qCWarning(logLuxUpdater) << "Unable to start" << appInfo.absoluteFilePath()
										<< "with arguments" << runArguments
										<< "as" << (adminAuth ? "admin/root" : "current user");
		}

		runOnExit = false;//prevent warning
	}
}
