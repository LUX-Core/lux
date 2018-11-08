#ifndef QTLUXUPDATER_UPDATECONTROLLER_H
#define QTLUXUPDATER_UPDATECONTROLLER_H

#include <qobject.h>
#include <qstringlist.h>
#include <qscopedpointer.h>

#include <qaction.h>

namespace QtLuxUpdater
{

class LuxUpdater;
class UpdateControllerPrivate;
//! A class to show a controlled update GUI to the user
class UpdateController : public QObject
{
	friend class UpdateControllerPrivate;
	Q_OBJECT

	//! Holds the path of the attached maintenancetool
	Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT FINAL)
	//! Holds the widget who's window should be used as parent for all dialogs
	Q_PROPERTY(QWidget* parentWindow READ parentWindow WRITE setParentWindow)
	//! Specifies whether the controller is currently active or not
	Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
	//! Specifies whether the controller should run the updater as admin or not
	Q_PROPERTY(bool runAsAdmin READ runAsAdmin WRITE setRunAsAdmin NOTIFY runAsAdminChanged)
	//! Holds the arguments to invoke the updater with
	Q_PROPERTY(QStringList updateRunArgs READ updateRunArgs WRITE setUpdateRunArgs RESET resetUpdateRunArgs)
	//! Specifies whether the update infos should be detailed or not
	Q_PROPERTY(bool detailedUpdateInfo READ isDetailedUpdateInfo WRITE setDetailedUpdateInfo)

public:
	//! Defines the different display-levels of the dialog
	enum DisplayLevel {
		AutomaticLevel = 0,//!< The lowest level. Nothing will be displayed at all. The programm will be auto-closed.
		ExitLevel = 1,/*!< The whole updating works completly automatically without displaying anything. Only
					   *   a notification that updates are ready to install will be shown if updates are available.
					   */
		InfoLevel = 2,//!< Will show information about updates if available, nothing otherwise.
		ExtendedInfoLevel = 3,//!< Will show information about the update result, for both cases, updates and no updates.
		ProgressLevel = 4,//!< Shows a (modal) progress dialog while checking for updates.
		AskLevel = 5,//!< Will ask the user if he wants to check for updates before actually checking.
        IgnoreLevel = 6//!< The highest level. Ignore asking user
	};
	Q_ENUM(DisplayLevel)

	//! Constructs a new controller with a parent. Will be application modal
	explicit UpdateController(QObject *parent = nullptr);
	//! Constructs a new controller with a parent. Will modal to the parent window
	explicit UpdateController(QWidget *parentWindow, QObject *parent = nullptr);
	//! Constructs a new controller with an explicitly set path and a parent. Will modal to the parent window
	explicit UpdateController(const QString &currentVersion, QObject *parent = nullptr);
	//! Constructs a new controller with an explicitly set path and a parent. Will be application modal
	explicit UpdateController(const QString &currentVersion, QWidget *parentWindow, QObject *parent = nullptr);
	~UpdateController();

	//! Create a QAction to start this controller from
	QAction *createUpdateAction(QObject *parent);

	//! @readAcFn{UpdateController::currentVersion}
	QString currentVersion() const;
	//! @readAcFn{UpdateController::parentWindow}
	QWidget* parentWindow() const;
	//! @readAcFn{UpdateController::currentDisplayLevel}
	DisplayLevel currentDisplayLevel() const;
	//! @readAcFn{UpdateController::running}
	bool isRunning() const;
	//! @readAcFn{UpdateController::runAsAdmin}
	bool runAsAdmin() const;
	//! @readAcFn{UpdateController::updateRunArgs}
	QStringList updateRunArgs() const;
	//! @readAcFn{UpdateController::detailedUpdateInfo}
	bool isDetailedUpdateInfo() const;

	//! Returns the Updater object used by the controller
	LuxUpdater *updater() const;

public Q_SLOTS:
	//! @writeAcFn{UpdateController::parentWindow}
	void setParentWindow(QWidget* parentWindow);
	//! @writeAcFn{UpdateController::runAsAdmin}
	void setRunAsAdmin(bool runAsAdmin, bool userEditable = true);
	//! @writeAcFn{UpdateController::updateRunArgs}
	void setUpdateRunArgs(QStringList updateRunArgs);
	//! @resetAcFn{UpdateController::updateRunArgs}
	void resetUpdateRunArgs();
	//! @writeAcFn{UpdateController::detailedUpdateInfo}
	void setDetailedUpdateInfo(bool detailedUpdateInfo);

	//! Starts the controller with the specified display level
	bool start(DisplayLevel displayLevel = InfoLevel);
	//! Tries to cancel the controllers update
	bool cancelUpdate(int maxDelay = 3000);

	//! Schedules an update after a specific delay, optionally repeated
	int scheduleUpdate(int delaySeconds, bool repeated = false, DisplayLevel displayLevel = InfoLevel);
	//! Schedules an update for a specific timepoint
	int scheduleUpdate(const QDateTime &when, DisplayLevel displayLevel = InfoLevel);
	//! Cancels the update with taskId
	void cancelScheduledUpdate(int taskId);

Q_SIGNALS:
	//! @notifyAcFn{UpdateController::running}
	void runningChanged(bool running);
	//! @notifyAcFn{UpdateController::runAsAdmin}
	void runAsAdminChanged(bool runAsAdmin);

private Q_SLOTS:
	void checkUpdatesDone(bool hasUpdates, bool hasError);
	void timerTriggered(const QVariant &parameter);

private:
	QScopedPointer<UpdateControllerPrivate> d;
};

}

#endif // QTLUXUPDATER_UPDATECONTROLLER_H
