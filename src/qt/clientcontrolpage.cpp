#include "clientcontrolpage.h"
#include "ui_clientcontrolpage.h"

#include "clientmodel.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "guiutil.h"
#include "guiconstants.h"

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QClipboard>
#include <QMessageBox>
#include <QMenu>

// Identify client version
#include <QDir>
#include <QTextStream>
#include <QProcess>

#define DECORATION_SIZE 48
#define NUM_ITEMS 10

ClientControlPage::ClientControlPage(QWidget *parent) :
    ui(new Ui::ClientControlPage),
    model(0)
{
    ui->setupUi(this);
   
    QFileInfo check_standalone(QDir::currentPath());
    if(!check_standalone.exists())
    {
       /*Client Update
       ui->chck4_upd8->setEnabled(false);
       ui->dwngrd_opt->setEnabled(false);
       ui->checkBoxupd8->setEnabled(false);
       ui->minCLIE->setEnabled(false);
       ui->CS_submit->setEnabled(false);
       ui->br_input->setEnabled(false);
       ui->BR_submit->setEnabled(false);
       ui->optin_test->setEnabled(false);
       ui->AU_apply->setEnabled(false);

        QMessageBox::warning(this, "Error",
                           "Local directory not found ---> Error 54",
                           QMessageBox::Ok );
    }
    else if(check_standalone.exists())
    {*/
        // TODO: Fix this up for OS porting
#ifdef Q_OS_WIN
        // READ INSTALLED VERSION
        QFile file(":/version");
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
            // error out if not accesable
           QMessageBox::information(0,"info",file.errorString());
        QTextStream in(&file);
        ui->instVER->setPlainText(in.readLine());

        // READ LIVE (CURRENT) VERSION
        //open a file
        QFile iniFILE(QDir::currentPath() + "/lUX.cfg");
       // if(!iniFILE.open(QIODevice::ReadOnly | QIODevice::Text))
            // error out if not accesable
           // QMessageBox::information(0,"info",iniFILE.errorString());
        QTextStream streamINI(&iniFILE);
        ui->dDIR->setPlainText(streamINI.readLine());

        QDir directory(ui->dDIR->toPlainText());
        QString curtxt = directory.filePath("version.check");
            iniFILE.close();

        QFile fileV(curtxt);
        //if(!fileV.open(QIODevice::ReadOnly | QIODevice::Text))
            // error out if not accesable
            //QMessageBox::information(0,"info",fileV.errorString());
        QTextStream inV(&fileV);
        ui->liveVER->setPlainText(inV.readAll());

        QFileInfo check_call(directory.filePath("upd8.skip"));
        if (check_call.exists() && check_call.isFile())
        {
            ui->checkBoxupd8->setChecked(false);
        }

        // Non-client Call
        else if(!check_call.exists())
        {
             ui->checkBoxupd8->setChecked(true);
        }
#endif
    }

}

ClientControlPage::~ClientControlPage()
{
    delete ui;
}


void ClientControlPage::on_chck4_upd8_clicked()
{
    QFile iniFILE(QDir::currentPath() + "/lUX.cfg");
    if(!iniFILE.open(QIODevice::ReadOnly | QIODevice::Text))
        // error out if not accesable
        QMessageBox::information(0,"info",iniFILE.errorString());
    QTextStream streamINI(&iniFILE);
    ui->dDIR->setPlainText(streamINI.readLine());

    QDir directory(ui->dDIR->toPlainText());
    QString genCheck = directory.filePath("version.call");
        iniFILE.close();

    QFile genCall(genCheck);
    if(genCall.open(QIODevice::ReadWrite))
    {
    QTextStream saturate(&genCall);
    saturate << "Temp File that flags verCheck.exe" << endl;
    }

    QProcess *process = new QProcess();
    QString fileX = QDir::currentPath() + "/lux-qt.exe";
    process->start(fileX);
}

void ClientControlPage::on_dwngrd_opt_clicked()
{
    QMessageBox::information(this, "Unavailable Versions",
                       "V2.3.0 is the minimum version, cannot be downgraded,",
                       QMessageBox::Ok );
}

void ClientControlPage::on_CS_submit_clicked()
{
    if(ui->minCLIE->isChecked())
    {
    QMessageBox::information(this, "Already Installed",
                       "You are already running the Minimal Client",
                       QMessageBox::Ok );
    this->ui->cliePREV->setText("Select a Client to Preview");

  //  if(ui->radioButtonViewLocalData->isChecked())
   //     {
        ui->minCLIE->setChecked(true);
        ui->minCLIE->setCheckable(true);
        ui->minCLIE->update();
        ui->minCLIE->setCheckable(true);
    //    }

    }
    else
    {
    QMessageBox::information(this, "You haven't selected anything yet",
                        "Please make a selection first.",
                        QMessageBox::Ok );
    }
}

void ClientControlPage::on_BR_submit_clicked()
{
    QMessageBox::information(this, "Coming in v2.3",
                       "Please email your error to contact@luxcoin.tect",
                       QMessageBox::Ok );
}

void ClientControlPage::on_AU_apply_clicked()
{
    QFile iniFILE(QDir::currentPath() + "/lUX.cfg");
    if(!iniFILE.open(QIODevice::ReadOnly | QIODevice::Text))
        // error out if not accesable
        QMessageBox::information(0,"info",iniFILE.errorString());
    QTextStream streamINI(&iniFILE);
    ui->dDIR->setPlainText(streamINI.readLine());

    QDir directory(ui->dDIR->toPlainText());
    QString upSkip = directory.filePath("upd8.skip");
    QString rmSKIP = directory.filePath("upd8.skip");
        iniFILE.close();

    if(ui->checkBoxupd8->isChecked())
    {
        if (QFile::exists(rmSKIP)) {
            QFile::remove(rmSKIP);
        }
    }
    else
    {
    // Skip Updating       
    QFile skipCall(upSkip);
    if(skipCall.open(QIODevice::ReadWrite))
    {
    QTextStream saturate(&skipCall);
    saturate << "Temp File that flags verCheck.exe" << endl;
    }
    }
}

void ClientControlPage::on_minCLIE_clicked()
{
    if(ui->minCLIE->isChecked())
    {
    QPixmap pix(":/images/cliecked");
    this->ui->cliePREV->setPixmap(pix);
    }
    else
    {
        //Do Nothing
    }
}
