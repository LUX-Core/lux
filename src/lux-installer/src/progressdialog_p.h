#ifndef QTLUXUPDATER_PROGRESSDIALOG_P_H
#define QTLUXUPDATER_PROGRESSDIALOG_P_H

#include <QScopedPointer>

#include <QDialog>
#include <QMessageBox>

#if defined(Q_OS_WIN) && defined(QT_WINEXTRAS)
#include <QtWinExtras/QWinTaskbarButton>
#endif

#include <functional>

namespace Ui {
class ProgressDialog;
}

namespace QtLuxUpdater
{

class ProgressDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ProgressDialog(QWidget *parent = nullptr);
	~ProgressDialog();
    void hideProgressDialog(QMessageBox::Icon hideType);

	template <class Class>
	void open(Class *object, void(Class::* member)(int,bool)) {
		connect(this, &ProgressDialog::canceled, object, [=](){
			(object->*member)(3000, true);
		});
		show();
	}

	void setCanceled();

public Q_SLOTS:
	void accept() override {}
	void reject() override {}

	void hide(QMessageBox::Icon hideType);

Q_SIGNALS:
	void canceled();

protected:
#ifdef Q_OS_WIN
	void showEvent(QShowEvent *event) override;
#endif
	void closeEvent(QCloseEvent *event) override;

private:
	QScopedPointer<Ui::ProgressDialog> ui;
#ifdef Q_OS_WIN
#ifdef QT_WINEXTRAS
	QWinTaskbarButton *tButton;
#endif
	void setupTaskbar(QWidget *window);
#endif
};

}

#endif // QTLUXUPDATER_PROGRESSDIALOG_P_H
