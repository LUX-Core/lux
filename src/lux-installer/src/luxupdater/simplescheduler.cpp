#include "simplescheduler_p.h"
#include "luxupdater_p.h"

#include <QtCore/QTimerEvent>
#include <QtCore/QDebug>

using namespace QtLuxUpdater;

SimpleScheduler::SimpleScheduler(QObject *parent) :
	QObject(parent),
	timerHash()
{}

int SimpleScheduler::startSchedule(int msecs, bool repeated, const QVariant &parameter)
{
	if(msecs < 0) {
		qCWarning(logLuxUpdater) << "Cannot schedule update tasks for the past!";
		return 0;
	}

	const auto id = startTimer(msecs, Qt::VeryCoarseTimer);
	if(id != 0)
		timerHash.insert(id, {repeated, parameter});
	return id;
}

int SimpleScheduler::startSchedule(const QDateTime &when, const QVariant &parameter)
{
	const auto delta = QDateTime::currentDateTime().msecsTo(when);
	if(delta > (qint64)INT_MAX) {
		qCWarning(logLuxUpdater) << "Time interval to big, timepoint to far in the future.";
		return 0;
	} else
		return startSchedule((int)delta, false, parameter);
}

void SimpleScheduler::cancelSchedule(int id)
{
	killTimer(id);
	timerHash.remove(id);
}

void SimpleScheduler::timerEvent(QTimerEvent *event)
{
	const auto id = event->timerId();
	const auto info = timerHash.value(id, {false, QVariant()});
	if(!info.first)
		cancelSchedule(id);
	emit scheduleTriggered(info.second);
	event->accept();
}
