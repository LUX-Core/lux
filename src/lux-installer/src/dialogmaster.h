#ifndef MESSAGEMASTER_H
#define MESSAGEMASTER_H

#include <QDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QCheckBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QColorDialog>
#include <QFontDialog>
class QProgressDialog;

namespace DialogMaster
{
	// ---------- QDialog ----------
	static const Qt::WindowFlags DefaultFlags = (Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);

	void masterDialog(QDialog *dialog, bool fixedSize = false, Qt::WindowFlags additionalFlags = DialogMaster::DefaultFlags);

	// ---------- QMessageBox ----------
	struct MessageBoxIcon {
		bool isCustom;
		QMessageBox::Icon mbxIcon;
		QPixmap custIcon;

		MessageBoxIcon(QMessageBox::Icon mbxIcon = QMessageBox::NoIcon);
		MessageBoxIcon(const QPixmap &custIcon);
		MessageBoxIcon(const QIcon &custIcon);
	};

	struct MessageBoxInfo {
		QWidget *parent;
		MessageBoxIcon icon;
		QString text;
		QString title;
		QString windowTitle;
		QString details;
		bool *checked;
		QString checkString;
		QMessageBox::StandardButtons buttons;
		QMessageBox::StandardButton defaultButton;
		QMessageBox::StandardButton escapeButton;
		QHash<QMessageBox::StandardButton, QString> buttonTexts;

		MessageBoxInfo();
	};

	QMessageBox *createMessageBox(const MessageBoxInfo &setup);
	QMessageBox::StandardButton messageBox(const MessageBoxInfo &setup);



	QMessageBox::StandardButton information(QWidget *parent,
											const QString &text,
											const QString &title = QString(),
											const QString &windowTitle = QString(),
											QMessageBox::StandardButtons buttons = QMessageBox::Ok,
											QMessageBox::StandardButton defaultButton = QMessageBox::Ok,
											QMessageBox::StandardButton escapeButton = QMessageBox::Ok);

	QMessageBox::StandardButton informationT(QWidget *parent,
											 const QString &windowTitle,
											 const QString &text,
											 QMessageBox::StandardButtons buttons = QMessageBox::Ok,
											 QMessageBox::StandardButton defaultButton = QMessageBox::Ok,
											 QMessageBox::StandardButton escapeButton = QMessageBox::Ok);

	MessageBoxInfo createInformation(const QString &text = QString(), QWidget *parent = Q_NULLPTR);

	QMessageBox::StandardButton question(QWidget *parent,
										 const QString &text,
										 const QString &title = QString(),
										 const QString &windowTitle = QString(),
										 QMessageBox::StandardButtons buttons = QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No),
										 QMessageBox::StandardButton defaultButton = QMessageBox::Yes,
										 QMessageBox::StandardButton escapeButton = QMessageBox::No);

	QMessageBox::StandardButton questionT(QWidget *parent,
										 const QString &windowTitle,
										 const QString &text,
										 QMessageBox::StandardButtons buttons = QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No),
										 QMessageBox::StandardButton defaultButton = QMessageBox::Yes,
										 QMessageBox::StandardButton escapeButton = QMessageBox::No);

	MessageBoxInfo createQuestion(const QString &text = QString(), QWidget *parent = Q_NULLPTR);

	QMessageBox::StandardButton warning(QWidget *parent,
										const QString &text,
										const QString &title = QString(),
										const QString &windowTitle = QString(),
										QMessageBox::StandardButtons buttons = QMessageBox::Ok,
										QMessageBox::StandardButton defaultButton = QMessageBox::Ok,
										QMessageBox::StandardButton escapeButton = QMessageBox::Ok);

	QMessageBox::StandardButton warningT(QWidget *parent,
										 const QString &windowTitle,
										 const QString &text,
										 QMessageBox::StandardButtons buttons = QMessageBox::Ok,
										 QMessageBox::StandardButton defaultButton = QMessageBox::Ok,
										 QMessageBox::StandardButton escapeButton = QMessageBox::Ok);

	MessageBoxInfo createWarning(const QString &text = QString(), QWidget *parent = Q_NULLPTR);

	QMessageBox::StandardButton critical(QWidget *parent,
										 const QString &text,
										 const QString &title = QString(),
										 const QString &windowTitle = QString(),
										 QMessageBox::StandardButtons buttons = QMessageBox::Ok,
										 QMessageBox::StandardButton defaultButton = QMessageBox::Ok,
										 QMessageBox::StandardButton escapeButton = QMessageBox::Ok);

	QMessageBox::StandardButton criticalT(QWidget *parent,
										  const QString &windowTitle,
										  const QString &text,
										  QMessageBox::StandardButtons buttons = QMessageBox::Ok,
										  QMessageBox::StandardButton defaultButton = QMessageBox::Ok,
										  QMessageBox::StandardButton escapeButton = QMessageBox::Cancel);

	MessageBoxInfo createCritical(const QString &text = QString(), QWidget *parent = Q_NULLPTR);

	void about(QWidget *parent,
			   const QString &content,
			   const QUrl &websiteUrl = QUrl(),
			   const QString &licenseName = QString(),
			   const QUrl &licenseUrl = QUrl(),
			   const QString &companyName = QString(),
			   bool addQtInfo = true,
			   const QString &extraVersionInfo = QString());

	// ---------- QInputDialog ----------
	double getDouble(QWidget *parent,
					 const QString &label,
					 double value = 0,
					 double min = INT_MIN,
					 double max = INT_MAX,
					 const QString &title = QString(),
					 int decimals = 1,
					 bool *ok = Q_NULLPTR);

	int getInt(QWidget *parent,
			   const QString &label,
			   int value = 0,
			   int min = INT_MIN,
			   int max = INT_MAX,
			   const QString &title = QString(),
			   int step = 1,
			   bool *ok = 0);

	QString getItem(QWidget *parent,
					const QString &label,
					const QStringList &items,
					const QString &title = QString(),
					int current = 0,
					bool editable = true,
					bool *ok = Q_NULLPTR,
					Qt::InputMethodHints inputMethodHints = Qt::ImhNone);

	int getItemIndex(QWidget *parent,
					 const QString &label,
					 const QStringList &items,
					 const QString &title = QString(),
					 int current = 0,
					 bool *ok = Q_NULLPTR);

	QString getMultiLineText(QWidget *parent,
							 const QString &label,
							 const QString &title = QString(),
							 const QString &text = QString(),
							 bool *ok = Q_NULLPTR,
							 Qt::InputMethodHints inputMethodHints = Qt::ImhNone);

	QString getText(QWidget *parent,
					const QString &label,
					const QString &title,
					QLineEdit::EchoMode mode = QLineEdit::Normal,
					const QString &text = QString(),
					bool *ok = Q_NULLPTR,
					Qt::InputMethodHints inputMethodHints = Qt::ImhNone);

	// ---------- QProgressDialog ----------
	QProgressDialog *createProgress(QWidget *parent,
									const QString &label,
									const int max = 0,
									const int min = 0,
									bool allowCancel = true,
									const QString &windowTitle = QString(),
									int minimumDuration = 250,
									const QString &cancelButtonText = QString());

	// ---------- QFileDialog ----------
	QString getExistingDirectory(QWidget *parent = Q_NULLPTR,
								 const QString &caption = QString(),
								 const QString &dir = QString(),
								 QFileDialog::Options options = QFileDialog::ShowDirsOnly);

	QUrl getExistingDirectoryUrl(QWidget *parent = Q_NULLPTR,
								 const QString &caption = QString(),
								 const QUrl &dir = QUrl(),
								 QFileDialog::Options options = QFileDialog::ShowDirsOnly,
								 const QStringList &supportedSchemes = QStringList());

	QString getOpenFileName(QWidget *parent = Q_NULLPTR,
							const QString &caption = QString(),
							const QString &dir = QString(),
							const QString &filter = QString(),
							QString *selectedFilter = Q_NULLPTR,
							QFileDialog::Options options = QFileDialog::Options());

	QStringList getOpenFileNames(QWidget *parent = Q_NULLPTR,
								 const QString &caption = QString(),
								 const QString &dir = QString(),
								 const QString &filter = QString(),
								 QString *selectedFilter = Q_NULLPTR,
								 QFileDialog::Options options = QFileDialog::Options());

	QUrl getOpenFileUrl(QWidget *parent = Q_NULLPTR,
						const QString &caption = QString(),
						const QUrl &dir = QUrl(),
						const QString &filter = QString(),
						QString *selectedFilter = Q_NULLPTR,
						QFileDialog::Options options = QFileDialog::Options(),
						const QStringList &supportedSchemes = QStringList());

	QList<QUrl>	getOpenFileUrls(QWidget *parent = Q_NULLPTR,
								const QString &caption = QString(),
								const QUrl &dir = QUrl(),
								const QString &filter = QString(),
								QString *selectedFilter = Q_NULLPTR,
								QFileDialog::Options options = QFileDialog::Options(),
								const QStringList &supportedSchemes = QStringList());

	QString	getSaveFileName(QWidget *parent = Q_NULLPTR,
							const QString &caption = QString(),
							const QString &dir = QString(),
							const QString &filter = QString(),
							QString *selectedFilter = Q_NULLPTR,
							QFileDialog::Options options = QFileDialog::Options());

	QUrl getSaveFileUrl(QWidget *parent = Q_NULLPTR,
						const QString &caption = QString(),
						const QUrl &dir = QUrl(),
						const QString &filter = QString(),
						QString *selectedFilter = Q_NULLPTR,
						QFileDialog::Options options = QFileDialog::Options(),
						const QStringList &supportedSchemes = QStringList());

	// ---------- QColorDialog ----------
	QColor getColor(const QColor &initial = Qt::white,
					QWidget *parent = Q_NULLPTR,
					const QString &title = QString(),
					QColorDialog::ColorDialogOptions options = QColorDialog::ColorDialogOptions());

	// ---------- QFontDialog ----------
	QFont getFont(bool *ok,
				  const QFont &initial,
				  QWidget *parent = Q_NULLPTR,
				  const QString &title = QString(),
				  QFontDialog::FontDialogOptions options = QFontDialog::FontDialogOptions());
}

#endif // MESSAGEMASTER_H
