#ifndef QTLUXUPDATER_UPDATECONTROLLER_P_H
#define QTLUXUPDATER_UPDATECONTROLLER_P_H

#include "updatecontroller.h"
#include "updateinfodialog_p.h"
#include "progressdialog_p.h"

#include "luxupdater/luxupdater.h"
#include "updatecontroller.h"
#include "luxupdater/simplescheduler_p.h"

#include <QtCore/QPointer>
#include <atomic>

extern QtLuxUpdater::ProgressDialog *gUpdatesProgress;
extern std::atomic<bool> running;
extern std::atomic<bool> wasCanceled;
extern QPointer<QWidget> win;
extern QtLuxUpdater::UpdateController::DisplayLevel gDisplayLevel;
extern std::atomic<bool> isUpdaterRunning;


namespace QtLuxUpdater
{

class UpdateControllerPrivate
{
public:
	typedef QPair<UpdateController::DisplayLevel, bool> UpdateTask;

	static QIcon getUpdatesIcon();

	UpdateController *q;

	QPointer<QWidget> window;

	LuxUpdater *mainUpdater;
	bool runAdmin;
	bool adminUserEdit;
	QStringList runArgs;
	bool detailedInfo;

	QPointer<ProgressDialog> checkUpdatesProgress;

	SimpleScheduler *scheduler;

	UpdateControllerPrivate(UpdateController *q_ptr, QWidget *window);
	UpdateControllerPrivate(UpdateController *q_ptr, const QString &version, QWidget *window);
	~UpdateControllerPrivate();
};

}

#endif // QTLUXUPDATER_UPDATECONTROLLER_P_H
