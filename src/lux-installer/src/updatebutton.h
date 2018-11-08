#ifndef QTLUXUPDATER_UPDATEBUTTON_H
#define QTLUXUPDATER_UPDATEBUTTON_H

#include "updatecontroller.h"

#include <qscopedpointer.h>
#include <qwidget.h>

namespace QtLuxUpdater
{

class UpdateButtonPrivate;
//! A simple button for update checks
class UpdateButton : public QWidget
{
	friend class UpdateButtonPrivate;
	Q_OBJECT

	//! The file of the animation to be shown
	Q_PROPERTY(QString animationFile READ animationFile WRITE setAnimationFile RESET resetAnimationFile)
	//! Specifies whether a result should be shown within the button or not
	Q_PROPERTY(bool showResult READ isShowingResult WRITE setShowResult)
	//! The display level to start the controller with
	Q_PROPERTY(UpdateController::DisplayLevel displayLevel READ displayLevel WRITE setDisplayLevel)
	//! The update controller this button works with
	Q_PROPERTY(UpdateController* controller READ controller WRITE setController NOTIFY controllerChanged)

public:
	//! Creates a new update button to place in your GUI
	explicit UpdateButton(QWidget *parent = nullptr, UpdateController *controller = nullptr);
	~UpdateButton();

	//! @readAcFn{UpdateButton::animationFile}
	QString animationFile() const;
	//! @readAcFn{UpdateButton::showResult}
	bool isShowingResult() const;
	//! @readAcFn{UpdateButton::displayLevel}
	UpdateController::DisplayLevel displayLevel() const;
	//! @readAcFn{UpdateButton::controller}
	UpdateController* controller() const;

public Q_SLOTS:
	//! Rests the buttons visual state
	void resetState();

	//! @writeAcFn{UpdateButton::animationFile}
	void setAnimationFile(QString animationFile, int speed = 100);
	//! @writeAcFn{UpdateButton::animationFile}
	void setAnimationDevice(QIODevice *animationDevice, int speed = 100);
	//! @resetAcFn{UpdateButton::animationFile}
	void resetAnimationFile();
	//! @writeAcFn{UpdateButton::showResult}
	void setShowResult(bool showResult);
	//! @writeAcFn{UpdateButton::displayLevel}
	void setDisplayLevel(UpdateController::DisplayLevel displayLevel);
	//! @writeAcFn{UpdateButton::controller}
	bool setController(UpdateController* controller);

Q_SIGNALS:
	//! @notifyAcFn{UpdateButton::controller}
	void controllerChanged(UpdateController* controller);

private Q_SLOTS:
	void startUpdate();
	void changeUpdaterState(bool isRunning);
	void updatesReady(bool hasUpdate, bool);
	void controllerDestroyed();

private:
	QScopedPointer<UpdateButtonPrivate> d;
};

}

#endif // QTLUXUPDATER_UPDATEBUTTON_H
