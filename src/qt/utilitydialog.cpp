// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The LUX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "utilitydialog.h"

#include "ui_helpmessagedialog.h"

#include "bitcoingui.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "intro.h"
#include "paymentrequestplus.h"
#include "guiutil.h"

#include "clientversion.h"
#include "init.h"
#include "util.h"

#include <stdio.h>

#include <QCloseEvent>
#include <QLabel>
#include <QRegExp>
#include <QTextTable>
#include <QTextCursor>
#include <QVBoxLayout>
#include <QSettings>
#include <QPixmap>

/** "Help message" or "About" dialog box */
HelpMessageDialog::HelpMessageDialog(QWidget* parent, HelpMode helpMode) : QDialog(parent),
                                                                    ui(new Ui::HelpMessageDialog)
{
    ui->setupUi(this);
	QSettings settings;
    QString version = tr("Luxcore") + " " + tr("version") + " " + QString::fromStdString(FormatFullVersion());
/* On x86 add a bit specifier to the version so that users can distinguish between
     * 32 and 64 bit builds. On other architectures, 32/64 bit may be more ambigious.
     */
#if defined(__x86_64__)
    version += " " + tr("(%1-bit)").arg(64);
#elif defined(__i386__)
    version += " " + tr("(%1-bit)").arg(32);
#endif

    if (helpMode == about) {
        setWindowTitle(tr("About Luxcore"));

        /// HTML-format the license message from the core
        QString licenseInfo = QString::fromStdString(LicenseInfo());
        QString licenseInfoHTML = licenseInfo;

        // Make URLs clickable
        QRegExp uri("<(.*)>", Qt::CaseSensitive, QRegExp::RegExp2);
        uri.setMinimal(true); // use non-greedy matching
		
		if (settings.value("theme").toString() == "dark grey") {
			licenseInfoHTML.replace(uri, "<a href=\"\\1\"><span style=\"color:#fff5f5;\">\\1</span></a>");
		} else if (settings.value("theme").toString() == "dark blue") {
			licenseInfoHTML.replace(uri, "<a href=\"\\1\"><span style=\"color:#fff5f5;\">\\1</span></a>");
		} else { 
			licenseInfoHTML.replace(uri, "<a href=\"\\1\">\\1</a>");
		}        
        // Replace newlines with HTML breaks
        licenseInfoHTML.replace("\n", "<br>");

        ui->aboutMessage->setTextFormat(Qt::RichText);
        ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        text = version + "\n" + licenseInfo;
        ui->aboutMessage->setText(version + "<br><br>" + licenseInfoHTML);
        ui->aboutMessage->setWordWrap(true);
        ui->helpMessage->setVisible(false);
        ui->findFrame->setVisible(false);
    } else if (helpMode == cmdline) {
    	ui->findMessage->setStyleSheet("QLabel { color : red; }");
        setWindowTitle(tr("Command-line options"));
        QString header = tr("Usage:") + "\n" +
                         "  lux-qt [" + tr("command-line options") + "]                     " + "\n";
        QTextCursor cursor(ui->helpMessage->document());
        cursor.insertText(version);
        cursor.insertBlock();
        cursor.insertText(header);
        cursor.insertBlock();

        std::string strUsage = HelpMessage(HMM_BITCOIN_QT);
        const bool showDebug = GetBoolArg("-help-debug", false);
        strUsage += HelpMessageGroup(tr("UI Options:").toStdString());
        if (showDebug) {
            strUsage += HelpMessageOpt("-allowselfsignedrootcertificates", strprintf("Allow self signed root certificates (default: %u)", DEFAULT_SELFSIGNED_ROOTCERTS));
        }
        strUsage += HelpMessageOpt("-choosedatadir", strprintf(tr("Choose data directory on startup (default: %u)").toStdString(), DEFAULT_CHOOSE_DATADIR));
        strUsage += HelpMessageOpt("-lang=<lang>", tr("Set language, for example \"de_DE\" (default: system locale)").toStdString());
        strUsage += HelpMessageOpt("-min", tr("Start minimized").toStdString());
        strUsage += HelpMessageOpt("-rootcertificates=<file>", tr("Set SSL root certificates for payment request (default: -system-)").toStdString());
        strUsage += HelpMessageOpt("-splash", strprintf(tr("Show splash screen on startup (default: %u)").toStdString(), DEFAULT_SPLASHSCREEN));
        strUsage += HelpMessageOpt("-resetguisettings", tr("Reset all settings changed in the GUI").toStdString());
        if (showDebug) {
            strUsage += HelpMessageOpt("-uiplatform", strprintf("Select platform to customize UI for (one of windows, macosx, other; default: %s)", BitcoinGUI::DEFAULT_UIPLATFORM));
        }
        QString coreOptions = QString::fromStdString(strUsage);
        text = version + "\n" + header + "\n" + coreOptions;

        QTextTableFormat tf;
        tf.setBorderStyle(QTextFrameFormat::BorderStyle_None);
        tf.setCellPadding(2);
        QVector<QTextLength> widths;
        widths << QTextLength(QTextLength::PercentageLength, 35);
        widths << QTextLength(QTextLength::PercentageLength, 65);
        tf.setColumnWidthConstraints(widths);

        QTextCharFormat bold;
        bold.setFontWeight(QFont::Bold);

        Q_FOREACH (const QString &line, coreOptions.split("\n")) {
            if (line.startsWith("  -"))
            {
                cursor.currentTable()->appendRows(1);
                cursor.movePosition(QTextCursor::PreviousCell);
                cursor.movePosition(QTextCursor::NextRow);
                cursor.insertText(line.trimmed());
                cursor.movePosition(QTextCursor::NextCell);
            } else if (line.startsWith("   ")) {
                cursor.insertText(line.trimmed()+' ');
            } else if (line.size() > 0) {
                //Title of a group
                if (cursor.currentTable())
                    cursor.currentTable()->appendRows(1);
                cursor.movePosition(QTextCursor::Down);
                cursor.insertText(line.trimmed(), bold);
                cursor.insertTable(1, 2, tf);
            }
        }

        ui->helpMessage->moveCursor(QTextCursor::Start);
        ui->scrollArea->setVisible(false);
        ui->aboutMessage->setVisible(false);

        connect(ui->findButton, &QPushButton::clicked, [&](){
            bool found = ui->helpMessage->find(ui->findLineEdit->text());
            ui->findMessage->clear();
            if(!found) {
                QTextCursor curCursor = ui->helpMessage->textCursor();
                curCursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor,1);
                ui->helpMessage->setTextCursor(curCursor);
                ui->findMessage->setText("No more results");
            }
        });
    }

	if (settings.value("theme").toString() == "dark grey") {
		QString styleSheet = ".QTextEdit { border: 0px; border-left: 1px solid #fff5f5;}";
		ui->helpMessage->setStyleSheet(styleSheet);
		ui->graphic->setPixmap(QPixmap(":/images/about-white"));
	} else if (settings.value("theme").toString() == "dark blue") {
		QString styleSheet = ".QTextEdit { border: 0px; border-left: 1px solid #fff5f5;}";
		ui->helpMessage->setStyleSheet(styleSheet);
		ui->graphic->setPixmap(QPixmap(":/images/about-white"));
	} else { 
		ui->graphic->setPixmap(QPixmap(":/images/about"));
	}
}

HelpMessageDialog::~HelpMessageDialog()
{
    delete ui;
}

void HelpMessageDialog::printToConsole()
{
    // On other operating systems, the expected action is to print the message to the console.
    fprintf(stdout, "%s\n", qPrintable(text));
}

void HelpMessageDialog::showOrPrint()
{
#if defined(WIN32)
    // On Windows, show a message box, as there is no stderr/stdout in windowed applications
    exec();
#else
    // On other operating systems, print help text to console
    printToConsole();
#endif
}

void HelpMessageDialog::on_okButton_accepted()
{
    close();
}


/** "Shutdown" window */
ShutdownWindow::ShutdownWindow(QWidget* parent, Qt::WindowFlags f) : QWidget(parent, f)
{
	QLabel* shutdownText = new QLabel(
								tr("Luxcore is shutting down...") + "<br /><br />" +
								tr("Do not shut down the computer until this window disappears."));
	QLabel* shutdownLogo = new QLabel();
	shutdownText->setAlignment(Qt::AlignCenter);
	shutdownLogo->setAlignment(Qt::AlignCenter);
	
    QVBoxLayout* layout = new QVBoxLayout();
	
	QSettings settings;
	
	if (settings.value("theme").toString() == "dark grey") {
		shutdownText->setStyleSheet("background-color: #262626; color: #fff; border: 0px solid #fff5f5 !important;");
		shutdownLogo->setStyleSheet("background-color: #262626; color: #fff; border: 0px solid #fff5f5 !important;");		
		shutdownLogo->setPixmap(QPixmap(":/icons/warning-white")
								.scaled(70,70,Qt::KeepAspectRatio,
								Qt::SmoothTransformation));
	} else if (settings.value("theme").toString() == "dark blue") {
		shutdownText->setStyleSheet("background-color: #031d54; color: #fff; border: 0px solid #fff5f5 !important;");
		shutdownLogo->setStyleSheet("background-color: #031d54; color: #fff; border: 0px solid #fff5f5 !important;");
		shutdownLogo->setPixmap(QPixmap(":/icons/warning-white")
								.scaled(70,70,Qt::KeepAspectRatio,
								Qt::SmoothTransformation));

	} else { 
		//shutdownText->setStyleSheet("background-color: #ffffff; color: #000;");
		//shutdownLogo->setStyleSheet("background-color: #ffffff; color: #000;");
		shutdownLogo->setPixmap(QPixmap(":/icons/warning")
								.scaled(70,70,Qt::KeepAspectRatio,
								Qt::SmoothTransformation));

	}
	
	layout->addStretch();
	layout->addWidget(shutdownLogo);
	layout->addStretch();
	layout->addWidget(shutdownText);
	layout->addStretch();
	
    setLayout(layout);
}

QWidget *ShutdownWindow::showShutdownWindow(BitcoinGUI* window)
{
    if (!window)
        return nullptr;

    // Show a simple window indicating shutdown status
    QWidget* shutdownWindow = new ShutdownWindow();
    shutdownWindow->setWindowTitle(window->windowTitle());
	
	QSettings settings;

	if (settings.value("theme").toString() == "dark grey") {
		shutdownWindow->setStyleSheet("background-color: #262626; border: 1px solid #fff5f5;");
	} else if (settings.value("theme").toString() == "dark blue") {
		shutdownWindow->setStyleSheet("background-color: #031d54; border: 1px solid #fff5f5;");
	} else { 
		shutdownWindow->setStyleSheet("background-color: #ffffff;");
	}

    // Center shutdown window at where main window was
    const QPoint global = window->mapToGlobal(window->rect().center());
    shutdownWindow->move(global.x() - shutdownWindow->width() / 2, global.y() - shutdownWindow->height() / 2);
    shutdownWindow->show();
    return shutdownWindow;
}

void ShutdownWindow::closeEvent(QCloseEvent* event)
{
    event->ignore();
}
