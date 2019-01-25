#include "updateinfodialog_p.h"
#include "forms/ui_updateinfodialog.h"
#include "updatecontroller_p.h"

#include <QApplication>
#include "dialogmaster.h"

using namespace QtLuxUpdater;

UpdateInfoDialog::UpdateInfoDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::UpdateInfoDialog)
{
	ui->setupUi(this);
	DialogMaster::masterDialog(this);
	ui->rootLayout->setSpacing(ui->rootLayout->contentsMargins().left());

#ifdef Q_OS_OSX
	QFont font = ui->headerLabel->font();
	font.setPointSize(20);
	ui->headerLabel->setFont(font);
#endif

	QPalette pal(ui->headerLabel->palette());
#ifdef Q_OS_WIN32
	// ### hardcoded for now:
	pal.setColor(QPalette::WindowText, QColor(0x00, 0x33, 0x99));
#else
	pal.setColor(QPalette::WindowText, QApplication::palette().color(QPalette::Highlight));
#endif
	ui->headerLabel->setPalette(pal);

	ui->headerLabel->setText(tr("Updates for %1 are available!")
								   .arg(QApplication::applicationDisplayName()));
	ui->imageLabel->setPixmap(UpdateControllerPrivate::getUpdatesIcon().pixmap(64, 64));

	// Releases url alternative
	QString url = "https://github.com/LUX-Core/lux/releases";
	ui->urlReleaseLabel->setText("<a href=\""+ url +"\">"+ url +"</a>");
	ui->urlReleaseLabel->setTextFormat(Qt::RichText);
	ui->urlReleaseLabel->setOpenExternalLinks(true);
	ui->urlReleaseLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
}

UpdateInfoDialog::~UpdateInfoDialog(){}

UpdateInfoDialog::DialogResult UpdateInfoDialog::showUpdateInfo(QList<LuxUpdater::LuxUpdateInfo> updates, bool &runAsAdmin, bool editable, bool detailed, QWidget *parent)
{
	if(!detailed) {
		DialogMaster::MessageBoxInfo boxInfo;
		boxInfo.parent = parent;
		boxInfo.windowTitle = tr("Check for Updates");
		boxInfo.icon = UpdateControllerPrivate::getUpdatesIcon();
		boxInfo.title = tr("Updates for %1 are available!")
						.arg(QApplication::applicationDisplayName());
		boxInfo.text = tr("There are new updates available! You can install them now or later.");
		QStringList details;
		for(auto info : updates) {
			details << QString("%1 v%2 — %3")
					   .arg(info.name)
					   .arg(info.version.toUtf8().constData())
					   .arg(getByteText(info.size));
		}
		boxInfo.details = details.join(QLatin1Char('\n'));

		boxInfo.checked = &runAsAdmin;
		boxInfo.checkString = tr("Run with &elevated rights");

		boxInfo.buttonTexts.insert(QMessageBox::Ok, tr("Install Now"));
		boxInfo.buttonTexts.insert(QMessageBox::Apply, tr("Install On Exit"));
		boxInfo.buttonTexts.insert(QMessageBox::Cancel, tr("Install later"));

		QScopedPointer<QMessageBox> box(DialogMaster::createMessageBox(boxInfo));
		box->checkBox()->setEnabled(editable);

		switch (box->exec()) {
		case QMessageBox::Ok:
			return InstallNow;
		case QMessageBox::Apply:
			return InstallLater;
		case QMessageBox::Cancel:
			return NoInstall;
		default:
			Q_UNREACHABLE();
		}
	} else {
		UpdateInfoDialog dialog(parent);

		for(auto info : updates) {
			auto item = new QTreeWidgetItem(dialog.ui->updateListTreeWidget);
			item->setText(0, info.name);
			item->setText(1, info.version.toUtf8().constData());
			item->setText(2, getByteText(info.size));
			item->setToolTip(2, tr("%L1 Bytes").arg(info.size));
		}
		dialog.ui->updateListTreeWidget->setColumnWidth(0, 120);
		dialog.ui->updateListTreeWidget->setColumnWidth(1, 80);
		dialog.ui->updateListTreeWidget->resizeColumnToContents(2);

		dialog.ui->runAdminCheckBox->setEnabled(editable);
		dialog.ui->runAdminCheckBox->setChecked(runAsAdmin);

		auto res = (DialogResult)dialog.exec();
		if(editable && res != NoInstall)
			runAsAdmin = dialog.ui->runAdminCheckBox->isChecked();
		return res;
	}
}

void QtLuxUpdater::UpdateInfoDialog::on_acceptButton_clicked()
{
	if(DialogMaster::questionT(this,
							   tr("Install Now?"),
							   tr("Close the application and install updates?"))
		== QMessageBox::Yes) {
		accept();
	}
}

void QtLuxUpdater::UpdateInfoDialog::on_delayButton_clicked()
{
	DialogMaster::informationT(this,
							   tr("Install On Exit"),
							   tr("Updates will be installed on exit. The maintenance tool "
								  "will be started as soon as you close the application!"));
	done(InstallLater);
}

QString UpdateInfoDialog::getByteText(qint64 bytes)
{
	auto counter = 0;
	auto disNum = (double)bytes;

	while((bytes / 1024) > 0 && counter < 3) {
		disNum = bytes / 1024.;
		bytes = disNum;
		++counter;
	}

	switch(counter) {
	case 0:
		return tr("%L1 Bytes").arg(bytes);
	case 1:
		return tr("%L1 KiB").arg(disNum, 0, 'f', 2);
	case 2:
		return tr("%L1 MiB").arg(disNum, 0, 'f', 2);
	case 3:
		return tr("%L1 GiB").arg(disNum, 0, 'f', 2);
	default:
		Q_UNREACHABLE();
		return QString();
	}
}
