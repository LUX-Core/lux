#include "dialogmaster.h"
#include <QCoreApplication>
#include <QPushButton>
#include <QProgressDialog>
#include <QProgressBar>
#include <QFileDialog>
#include <QApplication>

class CloseFilter : public QObject
{
public:
	inline CloseFilter(QObject *parent) :
		QObject(parent)
	{}
	inline bool eventFilter(QObject *, QEvent *ev) Q_DECL_OVERRIDE {
		if(ev->type() == QEvent::Close) {
			ev->ignore();
			return true;
		} else
			return false;
	}
};

void DialogMaster::masterDialog(QDialog *dialog, bool fixedSize, Qt::WindowFlags additionalFlags)
{
	Qt::WindowFlags flags = (fixedSize ? Qt::MSWindowsFixedSizeDialogHint : Qt::Window) | additionalFlags;
	dialog->setSizeGripEnabled(!fixedSize);
	if(dialog->parentWidget()) {
		dialog->setWindowModality(Qt::WindowModal);
		flags |= Qt::Sheet;
	} else {
		dialog->setWindowModality(Qt::ApplicationModal);
		flags |= Qt::Dialog;
	}
	dialog->setWindowFlags(flags);
}

DialogMaster::MessageBoxInfo::MessageBoxInfo() :
	parent(Q_NULLPTR),
	icon(QMessageBox::NoIcon),
	text(),
	title(),
	windowTitle(),
	details(),
	checked(Q_NULLPTR),
	checkString(),
	buttons(),
	defaultButton(QMessageBox::Ok),
	escapeButton(QMessageBox::Cancel),
	buttonTexts()
{}

QMessageBox *DialogMaster::createMessageBox(const DialogMaster::MessageBoxInfo &setup)
{
	QMessageBox *msgBox = new QMessageBox(setup.parent);
	if(setup.icon.isCustom)
		msgBox->setIconPixmap(setup.icon.custIcon);
	else
		msgBox->setIcon(setup.icon.mbxIcon);
	msgBox->setWindowTitle(setup.windowTitle);
	if(setup.title.isEmpty())
		msgBox->setText(setup.text);
	else {
		msgBox->setText(QStringLiteral("<b>%1</b>").arg(setup.title));
		msgBox->setInformativeText(setup.text);
	}
	if(setup.checked) {
		QCheckBox *box = new QCheckBox(setup.checkString, msgBox);
		box->setChecked(*setup.checked);
		bool *checkedPointer = setup.checked;
		QObject::connect(box, &QCheckBox::toggled, msgBox, [checkedPointer](bool isChecked){
			*checkedPointer = isChecked;
		});
		msgBox->setCheckBox(box);
	}
	msgBox->setStandardButtons(setup.buttons);
	msgBox->setDetailedText(setup.details);

	for(QHash<QMessageBox::StandardButton, QString>::const_iterator it = setup.buttonTexts.constBegin(),
																	end = setup.buttonTexts.constEnd();
		it != end;
		++it) {
		QAbstractButton *btn = msgBox->button(it.key());
		if(!btn)
			btn = msgBox->addButton(it.key());
		btn->setText(it.value());
	}

	msgBox->setDefaultButton(setup.defaultButton);
	msgBox->setEscapeButton(setup.escapeButton);

#ifdef Q_OS_WINCE
	Qt::WindowFlags flags = DialogMaster::DefaultFlags;
	if(setup.buttons.testFlag(QMessageBox::Ok))
		flags |= Qt::WindowOkButtonHint;
	if(setup.buttons.testFlag(QMessageBox::Cancel))
		flags |= Qt::WindowCancelButtonHint;
	DialogMaster::masterDialog(msgBox, true, flags);
#else
	DialogMaster::masterDialog(msgBox, true);
#endif

	return msgBox;
}

QMessageBox::StandardButton DialogMaster::messageBox(const MessageBoxInfo &setup)
{
	QScopedPointer<QMessageBox> box(DialogMaster::createMessageBox(setup));
	return (QMessageBox::StandardButton)box->exec();
}

QMessageBox::StandardButton DialogMaster::information(QWidget *parent, const QString &text, const QString &title, const QString &windowTitle, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton, QMessageBox::StandardButton escapeButton)
{
	MessageBoxInfo info = DialogMaster::createInformation(text, parent);
	info.title = title;
	if(!windowTitle.isEmpty())
		info.windowTitle = windowTitle;
	info.buttons = buttons;
	info.defaultButton = defaultButton;
	info.escapeButton = escapeButton;
	return messageBox(info);
}

QMessageBox::StandardButton DialogMaster::informationT(QWidget *parent, const QString &windowTitle, const QString &text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton, QMessageBox::StandardButton escapeButton)
{
	MessageBoxInfo info = DialogMaster::createInformation(text, parent);
	info.windowTitle = windowTitle;
	info.buttons = buttons;
	info.defaultButton = defaultButton;
	info.escapeButton = escapeButton;
	return messageBox(info);
}

DialogMaster::MessageBoxInfo DialogMaster::createInformation(const QString &text, QWidget *parent)
{
	MessageBoxInfo info;
	info.parent = parent;
	info.icon = QMessageBox::Information;
	info.text = text;
	info.windowTitle = QCoreApplication::translate("DialogMaster", "Information");
	info.buttons = QMessageBox::Ok;
	info.defaultButton = QMessageBox::Ok;
	info.escapeButton = QMessageBox::Ok;
	return info;
}

QMessageBox::StandardButton DialogMaster::question(QWidget *parent, const QString &text, const QString &title, const QString &windowTitle, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton, QMessageBox::StandardButton escapeButton)
{
	MessageBoxInfo info = DialogMaster::createQuestion(text, parent);
	info.title = title;
	if(!windowTitle.isEmpty())
		info.windowTitle = windowTitle;
	info.buttons = buttons;
	info.defaultButton = defaultButton;
	info.escapeButton = escapeButton;
	return messageBox(info);
}

QMessageBox::StandardButton DialogMaster::questionT(QWidget *parent, const QString &windowTitle, const QString &text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton, QMessageBox::StandardButton escapeButton)
{
	MessageBoxInfo info = DialogMaster::createQuestion(text, parent);
	info.windowTitle = windowTitle;
	info.buttons = buttons;
	info.defaultButton = defaultButton;
	info.escapeButton = escapeButton;
	return messageBox(info);
}

DialogMaster::MessageBoxInfo DialogMaster::createQuestion(const QString &text, QWidget *parent)
{
	MessageBoxInfo info;
	info.parent = parent;
	info.icon = QMessageBox::Question;
	info.text = text;
	info.windowTitle = QCoreApplication::translate("DialogMaster", "Question");
	info.buttons = QMessageBox::Yes | QMessageBox::No;
	info.defaultButton = QMessageBox::Yes;
	info.escapeButton = QMessageBox::No;
	return info;
}

QMessageBox::StandardButton DialogMaster::warning(QWidget *parent, const QString &text, const QString &title, const QString &windowTitle, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton, QMessageBox::StandardButton escapeButton)
{
	MessageBoxInfo info = DialogMaster::createWarning(text, parent);
	info.title = title;
	if(!windowTitle.isEmpty())
		info.windowTitle = windowTitle;
	info.buttons = buttons;
	info.defaultButton = defaultButton;
	info.escapeButton = escapeButton;
	return messageBox(info);
}

QMessageBox::StandardButton DialogMaster::warningT(QWidget *parent, const QString &windowTitle, const QString &text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton, QMessageBox::StandardButton escapeButton)
{
	MessageBoxInfo info = DialogMaster::createWarning(text, parent);
	info.windowTitle = windowTitle;
	info.buttons = buttons;
	info.defaultButton = defaultButton;
	info.escapeButton = escapeButton;
	return messageBox(info);
}

DialogMaster::MessageBoxInfo DialogMaster::createWarning(const QString &text, QWidget *parent)
{
	MessageBoxInfo info;
	info.parent = parent;
	info.icon = QMessageBox::Warning;
	info.text = text;
	info.windowTitle = QCoreApplication::translate("DialogMaster", "Warning");
	info.buttons = QMessageBox::Ok;
	info.defaultButton = QMessageBox::Ok;
	info.escapeButton = QMessageBox::Ok;
	return info;
}

QMessageBox::StandardButton DialogMaster::critical(QWidget *parent, const QString &text, const QString &title, const QString &windowTitle, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton, QMessageBox::StandardButton escapeButton)
{
	MessageBoxInfo info = DialogMaster::createCritical(text, parent);
	info.title = title;
	if(!windowTitle.isEmpty())
		info.windowTitle = windowTitle;
	info.buttons = buttons;
	info.defaultButton = defaultButton;
	info.escapeButton = escapeButton;
	return messageBox(info);
}

QMessageBox::StandardButton DialogMaster::criticalT(QWidget *parent, const QString &windowTitle, const QString &text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton, QMessageBox::StandardButton escapeButton)
{
	MessageBoxInfo info = DialogMaster::createCritical(text, parent);
	info.windowTitle = windowTitle;
	info.buttons = buttons;
	info.defaultButton = defaultButton;
	info.escapeButton = escapeButton;
	return messageBox(info);
}

DialogMaster::MessageBoxInfo DialogMaster::createCritical(const QString &text, QWidget *parent)
{
	MessageBoxInfo info;
	info.parent = parent;
	info.icon = QMessageBox::Critical;
	info.text = text;
	info.windowTitle = QCoreApplication::translate("DialogMaster", "Error");
	info.buttons = QMessageBox::Ok;
	info.defaultButton = QMessageBox::Ok;
	info.escapeButton = QMessageBox::Ok;
	return info;
}

void DialogMaster::about(QWidget *parent, const QString &content, const QUrl &websiteUrl, const QString &licenseName, const QUrl &licenseUrl, const QString &companyName, bool addQtInfo, const QString &extraVersionInfo)
{
	auto info = DialogMaster::createInformation();
	info.parent = parent;
	auto appIcon = QApplication::windowIcon();
	if(!appIcon.isNull())
		info.icon = appIcon;
	info.windowTitle = QCoreApplication::translate("DialogMaster", "About");
	info.title = QCoreApplication::translate("DialogMaster", "%1 — Version %2")
				 .arg(QApplication::applicationDisplayName())
				 .arg(QApplication::applicationVersion());

	static const QString pBegin = QStringLiteral("<p>");
	static const QString pEnd = QStringLiteral("</p>");
	static const QString br = QStringLiteral("<br/>");

	//basic text
	info.text = pBegin + content + pEnd;
	//qt version info
	if(addQtInfo || !extraVersionInfo.isEmpty()) {
		info.text += pBegin;
		if(!extraVersionInfo.isEmpty()) {
			info.text += extraVersionInfo;
			if(addQtInfo)
				info.text += br;
		}
		if(addQtInfo) {
			info.text += QCoreApplication::translate("DialogMaster", "Qt-Version: <a href=\"https://www.qt.io/\">%2</a>")
						 .arg(QStringLiteral(QT_VERSION_STR));
		}
		info.text += pEnd;
	}
	//Developer info
	info.text += pBegin + QCoreApplication::translate("DialogMaster", "Developed by: %1")
				 .arg(companyName.isEmpty() ? QCoreApplication::organizationName() : companyName);
	if(websiteUrl.isValid()) {
		info.text += br + QCoreApplication::translate("DialogMaster", "Project Website: <a href=\"%1\">%2</a>")
					 .arg(QString::fromUtf8(websiteUrl.toEncoded()))
					 .arg(websiteUrl.toString());
	}
	if(!licenseName.isEmpty()) {
		if(licenseUrl.isValid()) {
			info.text += br + QCoreApplication::translate("DialogMaster", "License: <a href=\"%1\">%2</a>")
						 .arg(QString::fromUtf8(licenseUrl.toEncoded()))
						 .arg(licenseName);
		} else {
			info.text += br + QCoreApplication::translate("DialogMaster", "License: %1")
						 .arg(licenseName);
		}
	}
	info.text += pEnd;

	info.buttons = QMessageBox::Close;
	info.defaultButton = QMessageBox::Close;
	info.escapeButton = QMessageBox::Close;
	if(addQtInfo) {
		info.buttons |= QMessageBox::Help;
		info.buttonTexts.insert(QMessageBox::Help, QCoreApplication::translate("DialogMaster", "About Qt"));
	}

	auto btn = DialogMaster::messageBox(info);
	if(btn == QMessageBox::Help)
		QApplication::aboutQt();
}





DialogMaster::MessageBoxIcon::MessageBoxIcon(QMessageBox::Icon mbxIcon) :
	isCustom(false),
	mbxIcon(mbxIcon),
	custIcon()
{}

DialogMaster::MessageBoxIcon::MessageBoxIcon(const QPixmap &custIcon) :
	isCustom(true),
	mbxIcon(),
	custIcon(custIcon)
{}

DialogMaster::MessageBoxIcon::MessageBoxIcon(const QIcon &custIcon) :
	isCustom(true),
	mbxIcon(),
	custIcon(custIcon.pixmap(64, 64))
{}



double DialogMaster::getDouble(QWidget *parent, const QString &label, double value, double min, double max, const QString &title, int decimals, bool *ok)
{
	QInputDialog dialog(parent);
	dialog.setWindowTitle(title);
	dialog.setLabelText(label);
	dialog.setInputMode(QInputDialog::DoubleInput);
	dialog.setDoubleRange(min, max);
	dialog.setDoubleDecimals(decimals);
	dialog.setDoubleValue(value);

	DialogMaster::masterDialog(&dialog, true);
	if(dialog.exec() == QDialog::Accepted) {
		if(ok)
			*ok = true;
		return dialog.doubleValue();
	} else {
		if(ok)
			*ok = false;
		return value;
	}
}

int DialogMaster::getInt(QWidget *parent, const QString &label, int value, int min, int max, const QString &title, int step, bool *ok)
{
	QInputDialog dialog(parent);
	dialog.setWindowTitle(title);
	dialog.setLabelText(label);
	dialog.setInputMode(QInputDialog::IntInput);
	dialog.setIntRange(min, max);
	dialog.setIntStep(step);
	dialog.setIntValue(value);

	DialogMaster::masterDialog(&dialog, true);
	if(dialog.exec() == QDialog::Accepted) {
		if(ok)
			*ok = true;
		return dialog.intValue();
	} else {
		if(ok)
			*ok = false;
		return value;
	}
}

QString DialogMaster::getItem(QWidget *parent, const QString &label, const QStringList &items, const QString &title, int current, bool editable, bool *ok, Qt::InputMethodHints inputMethodHints)
{
	QInputDialog dialog(parent);
	dialog.setWindowTitle(title);
	dialog.setLabelText(label);
	dialog.setInputMode(QInputDialog::TextInput);
	dialog.setComboBoxItems(items);
	dialog.setComboBoxEditable(editable);
	dialog.setTextValue(items[current]);
	dialog.setInputMethodHints(inputMethodHints);
	dialog.setOptions(QInputDialog::UseListViewForComboBoxItems);

	DialogMaster::masterDialog(&dialog);
	if(dialog.exec() == QDialog::Accepted) {
		if(ok)
			*ok = true;
		return dialog.textValue();
	} else {
		if(ok)
			*ok = false;
		return items[current];
	}
}

int DialogMaster::getItemIndex(QWidget *parent, const QString &label, const QStringList &items, const QString &title, int current, bool *ok)
{
	QInputDialog dialog(parent);
	dialog.setWindowTitle(title);
	dialog.setLabelText(label);
	dialog.setInputMode(QInputDialog::TextInput);
	dialog.setComboBoxItems(items);
	dialog.setComboBoxEditable(false);
	dialog.setTextValue(items[current]);

	DialogMaster::masterDialog(&dialog, true);
	if(dialog.exec() == QDialog::Accepted) {
		if(ok)
			*ok = true;
		return items.indexOf(dialog.textValue());
	} else {
		if(ok)
			*ok = false;
		return current;
	}
}

QString DialogMaster::getMultiLineText(QWidget *parent, const QString &label, const QString &title, const QString &text, bool *ok, Qt::InputMethodHints inputMethodHints)
{
	QInputDialog dialog(parent);
	dialog.setWindowTitle(title);
	dialog.setLabelText(label);
	dialog.setInputMode(QInputDialog::TextInput);
	dialog.setTextValue(text);
	dialog.setInputMethodHints(inputMethodHints);
	dialog.setOption(QInputDialog::UsePlainTextEditForTextInput);

	DialogMaster::masterDialog(&dialog);
	if(dialog.exec() == QDialog::Accepted) {
		if(ok)
			*ok = true;
		return dialog.textValue();
	} else {
		if(ok)
			*ok = false;
		return text;
	}
}

QString DialogMaster::getText(QWidget *parent, const QString &label, const QString &title, QLineEdit::EchoMode mode, const QString &text, bool *ok, Qt::InputMethodHints inputMethodHints)
{
	QInputDialog dialog(parent);
	dialog.setWindowTitle(title);
	dialog.setLabelText(label);
	dialog.setInputMode(QInputDialog::TextInput);
	dialog.setTextEchoMode(mode);
	dialog.setTextValue(text);
	dialog.setInputMethodHints(inputMethodHints);

	DialogMaster::masterDialog(&dialog);
	if(dialog.exec() == QDialog::Accepted) {
		if(ok)
			*ok = true;
		return dialog.textValue();
	} else {
		if(ok)
			*ok = false;
		return text;
	}
}



QProgressDialog *DialogMaster::createProgress(QWidget *parent, const QString &label, const int max, const int min, bool allowCancel, const QString &windowTitle, int minimumDuration, const QString &cancelButtonText)
{
	QProgressDialog *dialog = new QProgressDialog(parent);
	DialogMaster::masterDialog(dialog, true, Qt::CustomizeWindowHint | Qt::WindowTitleHint);
	dialog->setWindowTitle(windowTitle.isEmpty() ?
							   (parent ? parent->windowTitle() : QString()) :
							   windowTitle);

	if(max == min && max == 0) {
		QProgressBar *bar = new QProgressBar(dialog);
		bar->setTextVisible(false);
		dialog->setBar(bar);
	}

	dialog->setLabelText(label);
	if(!allowCancel) {
		dialog->installEventFilter(new CloseFilter(dialog));
		dialog->setCancelButtonText(QString());
	} else if(!cancelButtonText.isEmpty())
		dialog->setCancelButtonText(cancelButtonText);
	dialog->setMinimumDuration(minimumDuration);
	dialog->setRange(min, max);
	dialog->setValue(min);

	return dialog;
}



QString DialogMaster::getExistingDirectory(QWidget *parent, const QString &caption, const QString &dir, QFileDialog::Options options)
{
	QFileDialog dialog(parent, caption);
	DialogMaster::masterDialog(&dialog);
	dialog.setWindowTitle(caption);

	dialog.setAcceptMode(QFileDialog::AcceptOpen);
	dialog.setFileMode(QFileDialog::Directory);
	dialog.setOptions(options);
	dialog.setDirectory(dir);

	if(dialog.exec() == QDialog::Accepted)
		return dialog.selectedFiles().first();
	else
		return QString();
}

QUrl DialogMaster::getExistingDirectoryUrl(QWidget *parent, const QString &caption, const QUrl &dir, QFileDialog::Options options, const QStringList &supportedSchemes)
{
	QFileDialog dialog(parent, caption);
	DialogMaster::masterDialog(&dialog);
	dialog.setWindowTitle(caption);

	dialog.setAcceptMode(QFileDialog::AcceptOpen);
	dialog.setFileMode(QFileDialog::Directory);
	dialog.setOptions(options);
	dialog.setDirectoryUrl(dir);
	dialog.setSupportedSchemes(supportedSchemes);

	if(dialog.exec() == QDialog::Accepted)
		return dialog.selectedUrls().first();
	else
		return QUrl();
}

QString DialogMaster::getOpenFileName(QWidget *parent, const QString &caption, const QString &dir, const QString &filter, QString *selectedFilter, QFileDialog::Options options)
{
	QFileDialog dialog(parent, caption);
	DialogMaster::masterDialog(&dialog);
	dialog.setWindowTitle(caption);

	dialog.setAcceptMode(QFileDialog::AcceptOpen);
	dialog.setFileMode(QFileDialog::ExistingFile);
	dialog.setOptions(options);
	dialog.setDirectory(dir);
	dialog.setNameFilter(filter);
	if(selectedFilter != Q_NULLPTR)
		dialog.selectNameFilter(*selectedFilter);

	if(dialog.exec() == QDialog::Accepted) {
		if(selectedFilter != Q_NULLPTR)
			*selectedFilter = dialog.selectedNameFilter();
		return dialog.selectedFiles().first();
	} else
		return QString();
}

QStringList DialogMaster::getOpenFileNames(QWidget *parent, const QString &caption, const QString &dir, const QString &filter, QString *selectedFilter, QFileDialog::Options options)
{
	QFileDialog dialog(parent, caption);
	DialogMaster::masterDialog(&dialog);
	dialog.setWindowTitle(caption);

	dialog.setAcceptMode(QFileDialog::AcceptOpen);
	dialog.setFileMode(QFileDialog::ExistingFiles);
	dialog.setOptions(options);
	dialog.setDirectory(dir);
	dialog.setNameFilter(filter);
	if(selectedFilter != Q_NULLPTR)
		dialog.selectNameFilter(*selectedFilter);

	if(dialog.exec() == QDialog::Accepted) {
		if(selectedFilter != Q_NULLPTR)
			*selectedFilter = dialog.selectedNameFilter();
		return dialog.selectedFiles();
	} else
		return QStringList();
}

QUrl DialogMaster::getOpenFileUrl(QWidget *parent, const QString &caption, const QUrl &dir, const QString &filter, QString *selectedFilter, QFileDialog::Options options, const QStringList &supportedSchemes)
{
	QFileDialog dialog(parent, caption);
	DialogMaster::masterDialog(&dialog);
	dialog.setWindowTitle(caption);

	dialog.setAcceptMode(QFileDialog::AcceptOpen);
	dialog.setFileMode(QFileDialog::ExistingFile);
	dialog.setOptions(options);
	dialog.setDirectoryUrl(dir);
	dialog.setSupportedSchemes(supportedSchemes);
	dialog.setNameFilter(filter);
	if(selectedFilter != Q_NULLPTR)
		dialog.selectNameFilter(*selectedFilter);

	if(dialog.exec() == QDialog::Accepted) {
		if(selectedFilter != Q_NULLPTR)
			*selectedFilter = dialog.selectedNameFilter();
		return dialog.selectedUrls().first();
	} else
		return QUrl();
}

QList<QUrl> DialogMaster::getOpenFileUrls(QWidget *parent, const QString &caption, const QUrl &dir, const QString &filter, QString *selectedFilter, QFileDialog::Options options, const QStringList &supportedSchemes)
{
	QFileDialog dialog(parent, caption);
	DialogMaster::masterDialog(&dialog);
	dialog.setWindowTitle(caption);

	dialog.setAcceptMode(QFileDialog::AcceptOpen);
	dialog.setFileMode(QFileDialog::ExistingFiles);
	dialog.setOptions(options);
	dialog.setDirectoryUrl(dir);
	dialog.setSupportedSchemes(supportedSchemes);
	dialog.setNameFilter(filter);
	if(selectedFilter != Q_NULLPTR)
		dialog.selectNameFilter(*selectedFilter);

	if(dialog.exec() == QDialog::Accepted) {
		if(selectedFilter != Q_NULLPTR)
			*selectedFilter = dialog.selectedNameFilter();
		return dialog.selectedUrls();
	} else
		return QList<QUrl>();
}

QString DialogMaster::getSaveFileName(QWidget *parent, const QString &caption, const QString &dir, const QString &filter, QString *selectedFilter, QFileDialog::Options options)
{
	QFileDialog dialog(parent, caption);
	DialogMaster::masterDialog(&dialog);
	dialog.setWindowTitle(caption);

	dialog.setAcceptMode(QFileDialog::AcceptSave);
	dialog.setFileMode(QFileDialog::AnyFile);
	dialog.setOptions(options);
	dialog.setDirectory(dir);
	dialog.setNameFilter(filter);
	if(selectedFilter != Q_NULLPTR)
		dialog.selectNameFilter(*selectedFilter);

	if(dialog.exec() == QDialog::Accepted) {
		if(selectedFilter != Q_NULLPTR)
			*selectedFilter = dialog.selectedNameFilter();
		return dialog.selectedFiles().first();
	} else
		return QString();
}

QUrl DialogMaster::getSaveFileUrl(QWidget *parent, const QString &caption, const QUrl &dir, const QString &filter, QString *selectedFilter, QFileDialog::Options options, const QStringList &supportedSchemes)
{
	QFileDialog dialog(parent, caption);
	DialogMaster::masterDialog(&dialog);
	dialog.setWindowTitle(caption);

	dialog.setAcceptMode(QFileDialog::AcceptSave);
	dialog.setFileMode(QFileDialog::AnyFile);
	dialog.setOptions(options);
	dialog.setDirectoryUrl(dir);
	dialog.setSupportedSchemes(supportedSchemes);
	dialog.setNameFilter(filter);
	if(selectedFilter != Q_NULLPTR)
		dialog.selectNameFilter(*selectedFilter);

	if(dialog.exec() == QDialog::Accepted) {
		if(selectedFilter != Q_NULLPTR)
			*selectedFilter = dialog.selectedNameFilter();
		return dialog.selectedUrls().first();
	} else
		return QUrl();
}



QColor DialogMaster::getColor(const QColor &initial, QWidget *parent, const QString &title, QColorDialog::ColorDialogOptions options)
{
	QColorDialog dialog(initial, parent);
	DialogMaster::masterDialog(&dialog, true);

	dialog.setWindowTitle(title);
	dialog.setOptions(options);

	if(dialog.exec() == QDialog::Accepted)
		return dialog.currentColor();
	else
		return QColor();
}



QFont DialogMaster::getFont(bool *ok, const QFont &initial, QWidget *parent, const QString &title, QFontDialog::FontDialogOptions options)
{
	QFontDialog dialog(initial, parent);
	DialogMaster::masterDialog(&dialog, true);

	dialog.setWindowTitle(title);
	dialog.setOptions(options);

	if(dialog.exec() == QDialog::Accepted) {
		if(ok)
			*ok = true;
		return dialog.currentFont();
	} else {
		if(ok)
			*ok = false;
		return QFont();
	}
}
