#include "updatecontroller.h"
#include "updatecontroller_p.h"
#include "adminauthorization_p.h"
#include "updatebutton.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include "dialogmaster.h"

#include "luxupdater/luxupdater_p.h"

std::atomic<bool> running(false);
std::atomic<bool> wasCanceled(false);
QPointer<QWidget> win;
QtLuxUpdater::UpdateController::DisplayLevel gDisplayLevel;
QtLuxUpdater::ProgressDialog *gUpdatesProgress;

using namespace QtLuxUpdater;

UpdateController::UpdateController(QObject *parent) :
	QObject(parent),
	d(new UpdateControllerPrivate(this, nullptr))
{}

UpdateController::UpdateController(QWidget *parentWidget, QObject *parent) :
	QObject(parent),
	d(new UpdateControllerPrivate(this, parentWidget))
{}

UpdateController::UpdateController(const QString &currentVersion, QObject *parent) :
	QObject(parent),
	d(new UpdateControllerPrivate(this, currentVersion, nullptr))
{}

UpdateController::UpdateController(const QString &currentVersion, QWidget *parentWidget, QObject *parent) :
	QObject(parent),
	d(new UpdateControllerPrivate(this, currentVersion, parentWidget))
{}

UpdateController::~UpdateController(){}

QAction *UpdateController::createUpdateAction(QObject *parent)
{
	auto updateAction = new QAction(UpdateControllerPrivate::getUpdatesIcon(),
									tr("Check for Updates"),
									parent);
	updateAction->setMenuRole(QAction::ApplicationSpecificRole);
	updateAction->setToolTip(tr("Checks if new updates are available. You will be prompted before updates are installed."));

	connect(updateAction, &QAction::triggered, this, [this](){
		start(UpdateController::ProgressLevel);
	});
	connect(this, &UpdateController::runningChanged,
			updateAction, &QAction::setDisabled);
	connect(this, &UpdateController::destroyed,
			updateAction, &QAction::deleteLater);

	return updateAction;
}

QString UpdateController::currentVersion() const
{
	return d->mainUpdater->currentVersion();
}

QWidget *UpdateController::parentWindow() const
{
	return d->window;
}

void UpdateController::setParentWindow(QWidget *parentWindow)
{
	d->window = parentWindow;
    win = parentWindow;
}

UpdateController::DisplayLevel UpdateController::currentDisplayLevel() const
{
	return gDisplayLevel;
}

bool UpdateController::isRunning() const
{
	return running;
}

bool UpdateController::runAsAdmin() const
{
	return d->runAdmin;
}

void UpdateController::setRunAsAdmin(bool runAsAdmin, bool userEditable)
{
	if(d->runAdmin != runAsAdmin) {
		d->runAdmin = runAsAdmin;
		if(d->mainUpdater->willRunOnExit())
			d->mainUpdater->runUpdaterOnExit(d->runAdmin ? new AdminAuthorization() : nullptr);
		emit runAsAdminChanged(runAsAdmin);
	}
	d->adminUserEdit = userEditable;
}

QStringList UpdateController::updateRunArgs() const
{
	return d->runArgs;
}

void UpdateController::setUpdateRunArgs(QStringList updateRunArgs)
{
	d->runArgs = updateRunArgs;
}

void UpdateController::resetUpdateRunArgs()
{
	d->runArgs = QStringList(QStringLiteral("--updater"));
}

bool UpdateController::isDetailedUpdateInfo() const
{
	return d->detailedInfo;
}

void UpdateController::setDetailedUpdateInfo(bool detailedUpdateInfo)
{
	d->detailedInfo = detailedUpdateInfo;
}

LuxUpdater *UpdateController::updater() const
{
	return d->mainUpdater;
}

bool UpdateController::start(DisplayLevel displayLevel)
{
	if(running)
	{
		DialogMaster::warningT(d->window,
								   tr("Check for Updates"),
								   tr("The program is already checking for updates!"));
		return false;
	}

	running = true;
	emit runningChanged(true);
	wasCanceled = false;
	gDisplayLevel = displayLevel;

	if(gDisplayLevel >= AskLevel) {
		if(DialogMaster::questionT(d->window,
								   tr("Check for Updates"),
								   tr("Do you want to check for updates now?"))
		   != QMessageBox::Yes) {
			running = false;
			emit runningChanged(false);
			return false;
		}
	}

	if(!d->mainUpdater->checkForUpdates()) {
		if(gDisplayLevel >= ProgressLevel) {
			DialogMaster::warningT(d->window,
								   tr("Check for Updates"),
								   tr("The program is already checking for updates!"));
		}
		running = false;
		emit runningChanged(false);
		return false;
	} else {
		if(gDisplayLevel >= ExtendedInfoLevel) {
            gUpdatesProgress = new ProgressDialog(d->window);
			d->checkUpdatesProgress = gUpdatesProgress;
			if(gDisplayLevel >= ProgressLevel) {
				connect(d->checkUpdatesProgress.data(), &ProgressDialog::canceled, this, [this](){
					wasCanceled = true;
				});
				d->checkUpdatesProgress->open(d->mainUpdater, &QtLuxUpdater::LuxUpdater::abortUpdateCheck);
			}
		}
		return true;
	}
	
}

bool UpdateController::cancelUpdate(int maxDelay)
{
	if(d->mainUpdater->isRunning()) {
		wasCanceled = true;
		if(d->checkUpdatesProgress)
			d->checkUpdatesProgress->setCanceled();
		d->mainUpdater->abortUpdateCheck(maxDelay, true);
		return true;
	} else
		return false;
}

int UpdateController::scheduleUpdate(int delaySeconds, bool repeated, UpdateController::DisplayLevel displayLevel)
{
	if((((qint64)delaySeconds) * 1000) > (qint64)INT_MAX) {
		qCWarning(logLuxUpdater) << "delaySeconds to big to be converted to msecs";
		return 0;
	}
	return d->scheduler->startSchedule(delaySeconds * 1000, repeated, QVariant::fromValue(displayLevel));
}

int UpdateController::scheduleUpdate(const QDateTime &when, UpdateController::DisplayLevel displayLevel)
{
	return d->scheduler->startSchedule(when, QVariant::fromValue(displayLevel));
}

void UpdateController::cancelScheduledUpdate(int taskId)
{
	d->scheduler->cancelSchedule(taskId);
}

void UpdateController::checkUpdatesDone(bool hasUpdates, bool hasError)
{
	if(gDisplayLevel >= ExtendedInfoLevel) {
		auto iconType = QMessageBox::NoIcon;
		if(hasUpdates)
			iconType = QMessageBox::Information;
		else {
			if(wasCanceled || !d->mainUpdater->exitedNormally())
				iconType = QMessageBox::Warning;
			else
				iconType = QMessageBox::Critical;
		}

		if(d->checkUpdatesProgress) {
			d->checkUpdatesProgress->hide(iconType);
			d->checkUpdatesProgress->deleteLater();
			d->checkUpdatesProgress = nullptr;
		}
	}

	if(wasCanceled) {
		if(gDisplayLevel >= ExtendedInfoLevel) {
			DialogMaster::warningT(d->window,
								   tr("Check for Updates"),
								   tr("Checking for updates was canceled!"));
		}
	} else {
		if(hasUpdates) {
			if(gDisplayLevel >= InfoLevel) {
				auto shouldShutDown = false;
				const auto oldRunAdmin = d->runAdmin;
				const auto res = UpdateInfoDialog::showUpdateInfo(d->mainUpdater->updateInfo(),
																  d->runAdmin,
																  d->adminUserEdit,
																  d->detailedInfo,
																  d->window);
				if(d->runAdmin != oldRunAdmin)
					emit runAsAdminChanged(d->runAdmin);

				QT_WARNING_PUSH
				QT_WARNING_DISABLE_GCC("-Wimplicit-fallthrough")
				switch(res) {
				case UpdateInfoDialog::InstallNow:
					shouldShutDown = true;
				case UpdateInfoDialog::InstallLater:
					d->mainUpdater->runUpdaterOnExit(d->runAdmin ? new AdminAuthorization() : nullptr);
					if(shouldShutDown)
						qApp->quit();
				case UpdateInfoDialog::NoInstall:
					break;
				default:
					Q_UNREACHABLE();
				}
				QT_WARNING_POP

			} else {
				d->mainUpdater->runUpdaterOnExit(d->runAdmin ? new AdminAuthorization() : nullptr);
				if(gDisplayLevel == ExitLevel) {
					DialogMaster::informationT(d->window,
											   tr("Install Updates"),
											   tr("New updates are available. The maintenance tool will be "
												  "started to install those as soon as you close the application!"));
				} else
					qApp->quit();
			}
		} else {
			if(hasError) {
				qCWarning(logLuxUpdater) << "maintenancetool process finished with exit code"
											<< d->mainUpdater->errorCode()
											<< "and error string:"
											<< d->mainUpdater->errorLog();
			}

			if(gDisplayLevel >= ExtendedInfoLevel) {
				if(d->mainUpdater->exitedNormally()) {
					DialogMaster::criticalT(d->window,
											tr("Check for Updates"),
											tr("No new updates available!"));
				} else {
					DialogMaster::warningT(d->window,
										   tr("Check for Updates"),
										   tr("The update process failed!"));
				}
			}
		}
	}

	running = false;
	emit runningChanged(false);
}

void UpdateController::timerTriggered(const QVariant &parameter)
{
	if(parameter.canConvert<DisplayLevel>())
		start(parameter.value<DisplayLevel>());
}

//-----------------PRIVATE IMPLEMENTATION-----------------

QIcon UpdateControllerPrivate::getUpdatesIcon()
{
	return QIcon::fromTheme(QStringLiteral("system-software-update"), QIcon(QStringLiteral(":/res/icons/luxupdate.ico")));
}

UpdateControllerPrivate::UpdateControllerPrivate(UpdateController *q_ptr, QWidget *window) :
	UpdateControllerPrivate(q_ptr, QString(), window)
{}

UpdateControllerPrivate::UpdateControllerPrivate(UpdateController *q_ptr, const QString &version, QWidget *window) :
	q(q_ptr),
	window(window),
	mainUpdater(version.isEmpty() ? new LuxUpdater(q_ptr) : new LuxUpdater(version, q_ptr)),
	runAdmin(true),
	adminUserEdit(true),
	runArgs(QStringLiteral("--updater")),
	detailedInfo(true),
	checkUpdatesProgress(nullptr),
	scheduler(new SimpleScheduler(q_ptr))
{
	running = false;
    gUpdatesProgress = nullptr;
	wasCanceled = false;
    gDisplayLevel = UpdateController::InfoLevel;
	
	QObject::connect(mainUpdater, &LuxUpdater::checkUpdatesDone,
					 q, &UpdateController::checkUpdatesDone,
					 Qt::QueuedConnection);
	QObject::connect(scheduler, &SimpleScheduler::scheduleTriggered,
					 q, &UpdateController::timerTriggered);

#ifdef Q_OS_UNIX
	QFileInfo appInfo(QCoreApplication::applicationFilePath());
	runAdmin = (appInfo.ownerId() == 0);
#endif
}

UpdateControllerPrivate::~UpdateControllerPrivate()
{
	if(running)
		qCWarning(logLuxUpdater) << "UpdaterController destroyed while still running! This can crash your application!";

	if(checkUpdatesProgress)
		checkUpdatesProgress->deleteLater();
}
