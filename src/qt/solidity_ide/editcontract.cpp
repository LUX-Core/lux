#include <QShortcut>
#include <QFile>
#include <QTemporaryDir>
#include <QMessageBox>
#include <QDebug>
#include <QTemporaryDir>
#include <QDir>
#include <QMetaEnum>
#include <QRegExp>
#include <QTextCursor>
#include <QToolTip>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPalette>
#include <QFileDialog>
#include <QLabel>
#include <QFontMetrics>
#include <quazip/JlCompress.h>
#include <QCoreApplication>
#include <QSortFilterProxyModel>

#include "scwizardwgt.h"
#include "allopenfilesmodel.h"
#include "editcontract.h"
#include "ui_editcontract.h"


//////////////////////////////////////////////////
///DEFINES
//////////////////////////////////////////////////

//settings
#define defSolcVersion "SolcVersion"

//file with local solc compilers
#define defLocalSolcFile "local"
#define defPathCompiler "path"
#define defVersionCompiler "version"

//default contract name
#define defContractName "tmp"


//default projects path
#define defProjectsPath QCoreApplication::applicationDirPath() + "/Contracts"


//treeWidgetCurrentProject roles
#define defFileInfoRole Qt::UserRole + 1

//building project
#define defBuildingSolcPath  "BuildingSolcPath"
#define defBuildingSolcVersionPath  "BuildingSolcVersionPath"

//erros/warnings widget
#define defErrWarningsPadding  30

//listwidget errwarnings roles
#define defX_Shift_Role Qt::UserRole + 1
#define defY_Shift_Role Qt::UserRole + 2
#define defAbs_File_Path_Role Qt::UserRole + 3
#define defClickable_Item_Role Qt::UserRole + 4
#define defTmp_Role Qt::UserRole + 5

EditContract::EditContract(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::EditContract)
{
    ui->setupUi(this);
    tmpDir = new QTemporaryDir();
    QDir(tmpDir->path()).mkpath("output");

    //toolsFrame
    {
        //search
        {
            QShortcut * shortcut = new QShortcut(QKeySequence(tr("Ctrl+F")),
                                                 this);
            connect(shortcut, &QShortcut::activated,
                    this, &EditContract::slotSearchClicked);
            connect(ui->pushButtonSearch, &QPushButton::clicked,
                    this, &EditContract::slotSearchClicked);
        }

        connect(ui->pushButtonUndo, &QPushButton::clicked,
                ui->codeEdit, &CodeEditor::undo);
        connect(ui->pushButtonRedo, &QPushButton::clicked,
                ui->codeEdit, &CodeEditor::redo);

        //openFile
        {
            QShortcut * shortcutOpen = new QShortcut(QKeySequence(tr("Ctrl+O")),
                                                 this);
            connect(shortcutOpen, &QShortcut::activated,
                    this, &EditContract::slotClickOpenFile);
            connect(ui->pushButtonOpenFile, &QPushButton::clicked,
                    this, &EditContract::slotClickOpenFile);
        }

        //saveFile
        {
            QShortcut * shortcut = new QShortcut(QKeySequence(tr("Ctrl+S")),
                                                 this);
            connect(shortcut, &QShortcut::activated,
                    this, &EditContract::slotSaveFileClick);
            connect(ui->pushButtonSaveFile, &QPushButton::clicked,
                    this, &EditContract::slotSaveFileClick);
            ui->pushButtonSaveFile->setEnabled(false);
        }

        //saveAllFiles
        {
            QShortcut * shortcut = new QShortcut(QKeySequence(tr("Ctrl+Shift+S")),
                                                 this);
            connect(shortcut, &QShortcut::activated,
                    this, &EditContract::slotSaveAllFilesClick);
            connect(ui->pushButtonSaveAllFiles, &QPushButton::clicked,
                    this, &EditContract::slotSaveAllFilesClick);
            ui->pushButtonSaveAllFiles->setEnabled(false);
        }

        //saveAsFile
        connect(ui->pushButtonSaveFileAs, &QPushButton::clicked,
                this, &EditContract::slotSaveFileAsClick);
        
        //newFile
        {
            QShortcut * shortcut = new QShortcut(QKeySequence(tr("Ctrl+N")),
                                                 this);
            connect(shortcut, &QShortcut::activated,
                    this, &EditContract::slotNewFileClick);
            connect(ui->pushButtonNewFile, &QPushButton::clicked,
                    this, &EditContract::slotNewFileClick);
        }        

        //wizard smart contract
        {
            QShortcut * shortcut = new QShortcut(QKeySequence(tr("Ctrl+W")),
                                                 this);
            connect(shortcut, &QShortcut::activated,
                    this, &EditContract::slotWizardSCClick);
            connect(ui->pushButtonWizard, &QPushButton::clicked,
                    this, &EditContract::slotWizardSCClick);
        }
        //set tooltips
#ifdef __APPLE__
        ui->pushButtonSearch->setToolTip(tr("Search (Cmd+F)"));
        ui->pushButtonRedo->setToolTip(tr("Redo (Cmd+Y)"));
        ui->pushButtonUndo->setToolTip(tr("Undo (Cmd+Z)"));
        ui->pushButtonOpenFile->setToolTip(tr("Open (Cmd+O)"));
        ui->pushButtonSaveFile->setToolTip(tr("Save (Cmd+S)"));
        ui->pushButtonSaveAllFiles->setToolTip(tr("Save All (Cmd+Shift+S)"));
        ui->pushButtonNewFile->setToolTip(tr("New *.sol (Cmd+N)"));
        ui->pushButtonWizard->setToolTip(tr("SC Wizard (Cmd+W)"));
#else
        ui->pushButtonSearch->setToolTip(tr("Search (Ctrl+F)"));
        ui->pushButtonRedo->setToolTip(tr("Redo (Ctrl+Y)"));
        ui->pushButtonUndo->setToolTip(tr("Undo (Ctrl+Z)"));
        ui->pushButtonOpenFile->setToolTip(tr("Open (Ctrl+O)"));
        ui->pushButtonSaveFile->setToolTip(tr("Save (Ctrl+S)"));
        ui->pushButtonSaveAllFiles->setToolTip(tr("Save All (Ctrl+Shift+S)"));
        ui->pushButtonNewFile->setToolTip(tr("New *.sol (Ctrl+N)"));
        ui->pushButtonWizard->setToolTip(tr("SC Wizard (Ctrl+W)"));
#endif
        ui->pushButtonSaveFileAs->setToolTip(tr("Save As..."));
    }

    //buildFrame
    {
        QShortcut * shortcutBuild = new QShortcut(QKeySequence(tr("Ctrl+B")),
                                             this);
        connect(shortcutBuild, &QShortcut::activated,
                this, &EditContract::slotBuildClicked);
        connect(ui->pushButtonBuild, &QPushButton::clicked,
                this, &EditContract::slotBuildClicked);
#ifdef __APPLE__
        ui->pushButtonBuild->setToolTip(tr("Build (Cmd+B)"));
#else
        ui->pushButtonBuild->setToolTip(tr("Build (Ctrl+B)"));
#endif
        process_build = new QProcess(this);
        connect(process_build.data(), static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                this,
                [this](int exitCode, QProcess::ExitStatus exitStatus){ this->slotBuildFinished(exitCode, exitStatus);});
        connect(process_build.data(), &QProcess::errorOccurred,
                this, &EditContract::slotBuildProcError);
    }

    //find widget
    {
        ui->wgtFindResults->hide();
        connect(ui->wgtFindResults, &FindWidget::sigNeedHiding,
            this, [this](){ui->wgtFindResults->hide();});
        connect(ui->codeEdit, &CodeEditor::sigFindResults,
                ui->wgtFindResults, &FindWidget::slotReadyFindResults);
        connect(ui->wgtFindResults, &FindWidget::sigFindResultChoosed,
                ui->codeEdit, &CodeEditor::slotCurrentFindResultChanged);
        connect(ui->wgtFindResults, &FindWidget::sigFindResultChoosed,
                this, &EditContract::slotFindResultChoosed);
    }

    nam = new QNetworkAccessManager(this);
    #ifdef _WIN32
        getDownloadLinksSolc();
    #endif
    #ifdef __linux__
    //detect bUbuntu
    {
        process_linux_distrib = new QProcess(this);
        connect(process_linux_distrib.data(), static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                this, [this](int exitCode, QProcess::ExitStatus exitStatus){ this->slotProcDistribFinished(exitCode, exitStatus);});
        process_linux_distrib->start("lsb_release", QStringList() << "-i");
    }
    #endif


    //ui->comboBoxCompilerVersion
    {
        fillInCompilerVersions();
        connect(ui->comboBoxCompilerVersion, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
              [=](int index){ slotChooseNewCompiler(index); });
        slotChooseNewCompiler(ui->comboBoxCompilerVersion->currentIndex());
        QListView * view = qobject_cast<QListView *>(ui->comboBoxCompilerVersion->view());
        view->setSpacing(5);
    }

    //allOpenFiles widget
    {
        allFilesModel = new QSortFilterProxyModel(ui->listViewAllOpenFiles);
        auto* sourceModel = new AllOpenFilesModel(ui->listViewAllOpenFiles);
        allFilesModel->setSourceModel(sourceModel);
        allFilesModel->sort(0, Qt::AscendingOrder);
        ui->listViewAllOpenFiles->setModel(allFilesModel);

        connect(ui->listViewAllOpenFiles, &QListView::clicked,
                this, &EditContract::slotListAllOpenClickFile);
    }

    //CurrentProject widget
    {
        ui->treeWidgetCurrentProject->setColumnCount(1);
        connect(ui->treeWidgetCurrentProject, &QTreeWidget::itemClicked,
                this, &EditContract::slotTreeCurProjClickFile);
    }


    connect(ui->checkBoxOptimization, &QCheckBox::stateChanged,
            this, &EditContract::slotOptimizationStateChanged);

    ui->progressBarDownloadSolc->hide();
    ui->listWidgetErrWarnings->installEventFilter(this);
    connect(ui->codeEdit, &CodeEditor::contentsChange,
            this, &EditContract::slotSolcCodeChanged);

    connect(ui->pushButtonAddCompiler, &QPushButton::clicked,
            this, &EditContract::slotAddSolcManually);
    slotNewFileClick();
    //dont know width of splitter in this place - it is reason why
    //we get width of code editor 999999 (Of course this is more than required)
    ui->splitterEditCode->setSizes(QList<int>() <<200<<99999<<200);
}

void EditContract::slotWizardSCClick()
{
    if(sc_wizard_Wgt.isNull())
    {
        sc_wizard_Wgt = new ScWizardWgt(this);
        sc_wizard_Wgt->show();

    }
    else
        sc_wizard_Wgt->activateWindow();
}

//if will be clicked Save All will be saved all files
//without files to which was choosed unsaved
void EditContract::closeEvent(QCloseEvent *event)
{
    auto listEdit = allFilesModel->match(allFilesModel->index(0,0),
                                     AllOpenFilesModel::EditingDataRole,
                                     true, -1);
    if(listEdit.isEmpty())
    {
        event->accept();
        return;
    }

    bool bSaveAll = false;

    auto _saveFile = [this](QModelIndex index)
    {
        if(allFilesModel->data(index, AllOpenFilesModel::TmpDataRole).toBool())
        {
            const auto & fileData = qvariant_cast<EditFileData>(allFilesModel->data(index, AllOpenFilesModel::AllDataRole));
            QMessageBox::information(this, tr("Save as file"),
                                     fileData.fileInfo.fileName() + tr(" - is tmp file. Please input new filename."));
            slotSaveFileAsClick();
        }
        else
            slotSaveFileClick();
    };
    foreach(auto index, listEdit)
    {
        const auto & fileData = qvariant_cast<EditFileData>(allFilesModel->data(index, AllOpenFilesModel::AllDataRole));
        openEditFile(fileData.fileInfo, fileData.bTmp, false);
        update();
        if(bSaveAll)
        {
            _saveFile(index);
            continue;
        }
        QMessageBox msgBox(this);
        msgBox.setText(fileData.fileInfo.fileName() + tr(" has been modified. Do you want save it?"));
        msgBox.setInformativeText("Path of file - " + fileData.fileInfo.absoluteFilePath());
        QPushButton *dontSaveAllButton = msgBox.addButton(tr("Don't Save All"), QMessageBox::DestructiveRole);
        QPushButton *dontSaveButton = msgBox.addButton(tr("Don't Save"), QMessageBox::DestructiveRole);
        msgBox.setStandardButtons(QMessageBox::SaveAll | QMessageBox::Save | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        msgBox.exec();

        auto clickedButton = msgBox.clickedButton();

        if(dontSaveAllButton == clickedButton)
        {
            event->accept();
            return;
        }
        if(dontSaveButton == clickedButton)
            continue;
        if(msgBox.button(QMessageBox::Cancel) == clickedButton)
        {
            event->ignore();
            return;
        }
        if(msgBox.button(QMessageBox::Save) == clickedButton)
            _saveFile(index);
        if(msgBox.button(QMessageBox::SaveAll) == clickedButton)
        {
            bSaveAll = true;
            _saveFile(index);
        }
    }
    event->accept();
}

void EditContract::slotNewFileClick()
{
    NumTmpFiles++;
    openEditFile(QFileInfo(tmpDir->filePath(defContractName + QString::number(NumTmpFiles) + ".sol")),
                 true, true);
}

void EditContract::slotFindResultChoosed(QString nameFile)
{
    QFileInfo info(nameFile);
    openEditFile(info, !info.exists(), false);
}

void EditContract::slotSaveAllFilesClick()
{
    auto list = allFilesModel->match(allFilesModel->index(0,0),
                                     AllOpenFilesModel::EditingDataRole,
                                     true, -1);
    //save all standard files
    foreach(auto index, list)
    {
        if(allFilesModel->data(index, AllOpenFilesModel::TmpDataRole).toBool())
            continue;
        QString absPathFile = allFilesModel->data(index, AllOpenFilesModel::AbsFilePathRole).toString();
        QFile file(absPathFile);
        if(!file.open(QIODevice::WriteOnly))
        {
            QMessageBox::critical(this, tr("Save all file"),
                                  tr("Can not open file - ") + absPathFile);
            return;
        }
        file.write(ui->codeEdit->utf8DataOfFile(absPathFile));
        file.close();
        allFilesModel->setData(index, false, AllOpenFilesModel::EditingDataRole);
    }
    //ask user to save tmp files
    foreach(auto index, list)
    {
        if(!allFilesModel->data(index, AllOpenFilesModel::TmpDataRole).toBool())
            continue;
        QString idFile = allFilesModel->data(index, AllOpenFilesModel::AbsFilePathRole).toString();
        QFileInfo info(idFile);
        openEditFile(info,true, false);
        auto but = QMessageBox::question(this, tr("Save tmp file"),
                                         idFile + tr(" is tmp file. Do you want save it in disk?"),
                                         QMessageBox::Yes | QMessageBox::No | QMessageBox::NoToAll,
                                         QMessageBox::No);
        if(QMessageBox::Yes == but)
            slotSaveFileAsClick();
        if(QMessageBox::NoToAll == but)
            break;
    }

    ui->pushButtonSaveFile->setEnabled(false);
    ui->pushButtonSaveAllFiles->setEnabled(false);
    ui->labelFileName->setText(allFilesModel->data(ui->listViewAllOpenFiles->currentIndex()).toString());

}

void EditContract::slotSaveFileAsClick()
{
    QString fileAbsName = QFileDialog::getSaveFileName(this, tr("Save file as"),
                                                    QCoreApplication::applicationDirPath(),
                                                    "Solidity files (*.sol);;All files (*)");
    QString prevName = allFilesModel->data(ui->listViewAllOpenFiles->currentIndex(),
                                           AllOpenFilesModel::AbsFilePathRole).toString();
    if(fileAbsName.isEmpty())
        return;
    //create file and write data to it
    QFile file(fileAbsName);
    if(!file.open(QIODevice::WriteOnly))
    {
        QMessageBox::critical(this, tr("Save file as"),
                              tr("Can not create file - ") + fileAbsName);
        return;
    }
    file.write(ui->codeEdit->utf8DataOfFile(prevName));
    file.close();

    bool bRemoveTmp = allFilesModel->data(ui->listViewAllOpenFiles->currentIndex(),
                                          AllOpenFilesModel::TmpDataRole).toBool();

    if(bRemoveTmp)
        allFilesModel->removeRow(ui->listViewAllOpenFiles->currentIndex().row());

    auto fileInfo = QFileInfo(fileAbsName);
    bool bActiveSol = (prevName==activeSolFileData().fileInfo.absoluteFilePath());

    openEditFile(fileInfo, false, bActiveSol);
    unSaveChangesEditFile(false);

    //must be last action
    //because we can not remove current QTextDocument in codeEdit
    if(bRemoveTmp)
        ui->codeEdit->removeDocument(prevName);
}

void EditContract::slotSaveFileClick()
{
    QString abs_path = editingFileData().fileInfo.absoluteFilePath();
    QFile file(abs_path);
    if(!file.open(QIODevice::WriteOnly))
    {
        QMessageBox::critical(this, tr("Save file"),
                              tr("Can not open file - ") + abs_path);
        return;
    }
    file.write(ui->codeEdit->toPlainText().toUtf8());
    file.close();

    unSaveChangesEditFile(false);
}

void EditContract::unSaveChangesEditFile(bool bUnSave)
{
    int curRow = ui->listViewAllOpenFiles->currentIndex().row();
    const auto & index = allFilesModel->index(curRow,0);
    allFilesModel->setData(index, bUnSave, AllOpenFilesModel::EditingDataRole);

    ui->labelFileName->setText(allFilesModel->data(index).toString());

    bool bTmp = allFilesModel->data(index, AllOpenFilesModel::TmpDataRole).toBool();
    ui->pushButtonSaveFile->setEnabled(bUnSave && !bTmp);
    auto list = allFilesModel->match(allFilesModel->index(0,0),
                                     AllOpenFilesModel::EditingDataRole,
                                     true);
    ui->pushButtonSaveAllFiles->setEnabled(!list.isEmpty());
}

EditFileData EditContract::editingFileData()
{
    int iCurRow = ui->listViewAllOpenFiles->currentIndex().row();
    auto index = allFilesModel->index(iCurRow,0);
    return qvariant_cast<EditFileData>(allFilesModel->data(index, AllOpenFilesModel::AllDataRole));
}

EditFileData EditContract::activeSolFileData()
{
    auto activeSolItem = ui->treeWidgetCurrentProject->topLevelItem(_ActiveSol);
    return qvariant_cast<EditFileData>(activeSolItem->data(0, defFileInfoRole));
}

void EditContract::slotTreeCurProjClickFile(QTreeWidgetItem *item)
{
    int topIndex = ui->treeWidgetCurrentProject->indexOfTopLevelItem(item);

    //click to header - do nothing
    if(-1 != topIndex
          &&
       _ActiveSol != topIndex)
        return;

    auto fileData = qvariant_cast<EditFileData>(item->data(0,defFileInfoRole));
    openEditFile(fileData.fileInfo, false, false);

}

void EditContract::slotListAllOpenClickFile(const QModelIndex & index)
{
    EditFileData data = qvariant_cast<EditFileData>(allFilesModel->data(index, AllOpenFilesModel::AllDataRole));
    openEditFile(data.fileInfo, data.bTmp,
                 false);
}

void EditContract::fillInImports(QString absFilePath)
{
    const auto & listImports = parseImports(absFilePath);
    activeSolcImportPathes.clear();
    auto importsItem = ui->treeWidgetCurrentProject->topLevelItem(_UpdatesFolder);
    if(listImports.isEmpty())
    {
        if(importsItem)
        {
            ui->treeWidgetCurrentProject->takeTopLevelItem(_UpdatesFolder);
            delete importsItem;
        }
        return;
    }
    if(nullptr == importsItem)
    {
        importsItem = new QTreeWidgetItem(ui->treeWidgetCurrentProject,
                                          QStringList() << "Imports");
        ui->treeWidgetCurrentProject->insertTopLevelItem(_UpdatesFolder, importsItem);
    }

    foreach(auto childItem, importsItem->takeChildren())
        delete childItem;

    //add new
    foreach(auto fileName, listImports)
    {
        QFileInfo info(fileName);
        QTreeWidgetItem * item = new QTreeWidgetItem(importsItem,
                                                     QStringList() <<info.fileName());
        item->setToolTip(0, fileName);
        item->setData(0,defFileInfoRole,
                         QVariant::fromValue(EditFileData(info, false)));
        importsItem->addChild(item);
        if(!activeSolcImportPathes.contains(info.absolutePath()))
            activeSolcImportPathes << info.absolutePath();
    }
    ui->codeEdit->addCurrentProject(QStringList()<< absFilePath << listImports);
    ui->treeWidgetCurrentProject->model()->sort(0);
}

QStringList EditContract::parseImports(QString absFilePath)
{
    QStringList allImports;
    QStringList unprocessFiles = {absFilePath};
    while(!unprocessFiles.isEmpty())
    {
        QString procFile =unprocessFiles.takeFirst();
        QFileInfo info(procFile);
        QDir dir(info.path());
        QFile file(procFile);
        QRegExp rx ("\\b(import)\\b\\s+\"[^\"]+\"");
        if(file.open(QIODevice::ReadOnly))
        {
            QString text = file.readAll();
            int pos = 0;
            while ((pos = rx.indexIn(text, pos)) != -1)
            {
                QString newImport = rx.cap(0).remove("import")
                       .remove(' ').remove("\"");
                newImport = dir.cleanPath(dir.absoluteFilePath(newImport));
                if(!allImports.contains(newImport))
                {
                    allImports << newImport;
                    unprocessFiles << newImport;
                }
                pos += rx.matchedLength();
            }
        }
    }
    return allImports;
}

//index in proxy model
QModelIndex EditContract::indexAllOpenFiles(QString absFilePath)
{
    QModelIndex res;
    auto list = allFilesModel->match(allFilesModel->index(0,0),
                                     AllOpenFilesModel::AbsFilePathRole,
                                     absFilePath);
    if(!list.isEmpty())
        res = list.first();
    return res;
}


void EditContract::openEditFile(const QFileInfo & info, bool bTmp,
                                bool bActiveSol)
{
    bNewTextOpenFile = true;
    if(false == bTmp &&
       !ui->codeEdit->containsFile(info.absoluteFilePath()))
    {
        QFile file(info.absoluteFilePath());
        if(!file.open(QIODevice::ReadOnly))
        {
            QMessageBox::critical(this, tr("Open *.sol file"),
                                  tr("Can not open file - ") + info.absoluteFilePath());
            bNewTextOpenFile = false;
            return;
        }
        QString data = file.readAll();
        ui->codeEdit->setCurrentDocument(info.absoluteFilePath());
        ui->codeEdit->setPlainText(data);
    }
    else
        ui->codeEdit->setCurrentDocument(info.absoluteFilePath());

    EditFileData activeSolData(info, bTmp);
    QModelIndex index = indexAllOpenFiles(info.absoluteFilePath());

    //redraw files listview
    if( !index.isValid())
    {
        allFilesModel->insertRow(0);
        allFilesModel->setData(allFilesModel->index(0,0),
                               QVariant::fromValue(activeSolData),
                               AllOpenFilesModel::AllDataRole);
        index = indexAllOpenFiles(info.absoluteFilePath());
    }
    ui->listViewAllOpenFiles->setCurrentIndex(index);

    ui->labelFileName->setText(allFilesModel->data(index).toString());
    ui->labelFileName->setToolTip(allFilesModel->data(index, Qt::ToolTipRole).toString());

    //repaint  treeWidgetCurrentProject
    if(bActiveSol)
    {
        ui->treeWidgetCurrentProject->clear();
        QTreeWidgetItem * topItem = new QTreeWidgetItem(ui->treeWidgetCurrentProject,
                                                        QStringList() << info.fileName());
        topItem->setToolTip(0, allFilesModel->data(index, Qt::ToolTipRole).toString());
        topItem->setData(0,defFileInfoRole,
                         QVariant::fromValue(activeSolData));
        ui->treeWidgetCurrentProject->insertTopLevelItem(0, topItem);

        fillInImports(info.absoluteFilePath());
    }

    //set current item in treeWidgetCurrentProject
    {
        bool bFind = false;
        for(int i_top=0;
            i_top<ui->treeWidgetCurrentProject->topLevelItemCount();
            i_top++)
        {
            auto topItem = ui->treeWidgetCurrentProject->topLevelItem(i_top);
            EditFileData fileData = qvariant_cast<EditFileData>(topItem->data(0, defFileInfoRole));
            if(info.absoluteFilePath() == fileData.fileInfo.absoluteFilePath())
            {
                ui->treeWidgetCurrentProject->setCurrentItem(topItem);
                bFind = true;
                break;
            }
            for(int i_child=0;
                i_child<topItem->childCount();
                i_child++)
            {
                auto childItem = topItem->child(i_child);
                EditFileData fileData = qvariant_cast<EditFileData>(childItem->data(0, defFileInfoRole));
                if(info.absoluteFilePath() == fileData.fileInfo.absoluteFilePath())
                {
                    ui->treeWidgetCurrentProject->setCurrentItem(childItem);
                    bFind = true;
                    break;

                }

            }
            if(bFind)
                break;

        }
        if(!bFind)
            ui->treeWidgetCurrentProject->setCurrentItem(nullptr);
    }

    //saveButton enabled/disabled
    bool bEditingFile = allFilesModel->data(index, AllOpenFilesModel::EditingDataRole).toBool();
    bool bTmpFile= allFilesModel->data(index, AllOpenFilesModel::TmpDataRole).toBool();
    ui->pushButtonSaveFile->setEnabled(bEditingFile && !bTmpFile);
    bNewTextOpenFile = false;
}

void EditContract::slotSolcCodeChanged(int position, int charsRemoved, int charsAdded)
{
    Q_UNUSED(position)
    if(!bNewTextOpenFile &&
      (charsRemoved>0 || charsAdded>0))
    {
        unSaveChangesEditFile(true);
    }
}

void EditContract::slotClickOpenFile()
{
    QString openFile = QFileDialog::getOpenFileName(this, tr("Open file"),
                                                    QCoreApplication::applicationDirPath(),
                                                    "Solidity files (*.sol);;All files (*)");

    if(!openFile.isEmpty())
    {
        openEditFile(QFileInfo(openFile),false, true);
    }
}

//get download links of solc
//in https://github.com/ethereum/solidity/releases ubuntu and windows releases
void EditContract::getDownloadLinksSolc()
{
    QNetworkRequest req (QUrl("https://api.github.com/repos/ethereum/solidity/releases"));
    auto reply = nam->get(req);
    connect(reply, &QNetworkReply::finished,
            this, &EditContract::slotDownLinksSolcFinished);
}

void EditContract::slotAddSolcManually()
{
    QString exeName;
#ifdef __linux__
    exeName = "solc";
#endif
#ifdef _WIN32
    exeName = "solc.exe";
#endif
    QString newSolc = QFileDialog::getOpenFileName(this, tr("Add Solc Compiler"),QCoreApplication::applicationDirPath(),
                                 exeName);
    if(! newSolc.isEmpty())
    {
        QString version;
        //fill in version
        {
            QProcess proc;
            proc.start(newSolc, QStringList() << "--version");
            proc.waitForFinished(1000);
            QRegExp reg_exp("\\d{1,2}[.]\\d{1,2}[.]\\d{1,2}");
            QString output = proc.readAllStandardOutput();
            int pos = reg_exp.indexIn(output);
            if (pos != -1)
                version = output.mid(pos).remove("\n").remove("\r");
        }
        if(version.isEmpty())
        {
            QMessageBox::critical(this, tr("Add Solc Compiler"), tr("It is not solc compiler!"));
            return;
        }

        QString localSolcFileName = QCoreApplication::applicationDirPath() + "/" + strCompilePath + "/" + QString(defLocalSolcFile);
        QFile file(localSolcFileName);
        QJsonDocument documentOld;
        if(file.open(QIODevice::ReadOnly))
        {
            documentOld = QJsonDocument::fromJson(file.readAll());
            file.close();
        }
        QJsonArray array = documentOld.array();
        QJsonObject newObj;
        newObj[defPathCompiler] = newSolc;
        newObj[defVersionCompiler] = version;
        array.append(newObj);
        QJsonDocument documentNew(array);
        if(file.open(QIODevice::Append))
        {
            file.write(documentNew.toJson());
            file.close();
        }
        pathsSolc[version] = newSolc;
        ui->comboBoxCompilerVersion->addItem(version);
        customizeComboBoxCompiler(ui->comboBoxCompilerVersion->count() - 1, false);
        ui->comboBoxCompilerVersion->setCurrentText(version);
    }
}

void EditContract::slotDownLinksSolcFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());
    auto data = reply->readAll();
    QJsonDocument document = QJsonDocument::fromJson(data);
    auto array = document.array();
    for(int i=0; i<array.size(); i++)
    {
        QJsonObject object = array[i].toObject();
        QString nameVersion = object["tag_name"].toString().remove("v");
        if(nameVersion.size() > 2)
        {
            if(nameVersion.at(nameVersion.size()-2) == '.')
            {
                nameVersion.insert(nameVersion.size()-1, "0");
            }
        }
        QJsonArray assetsArray = object["assets"].toArray();
        for(int iAsset=0; iAsset<assetsArray.size(); iAsset++)
        {
            QJsonObject asset = assetsArray[iAsset].toObject();
            QString namePackage = asset["name"].toString();
#ifdef __linux__
            if(bUbuntu)
            {
                if(namePackage.contains("ubuntu"))
                {
                    QString downloadUrl = asset["browser_download_url"].toString();
                    downloadLinksSolc[nameVersion] = downloadUrl;
                    break;
                }
            }
#endif
#ifdef _WIN32
            if(namePackage.contains("windows"))
            {
                QString downloadUrl = asset["browser_download_url"].toString();
                downloadLinksSolc[nameVersion] = downloadUrl;
                break;
            }
#endif
        }
    }

    //fill in comboBoxCompilerVersion
    foreach(QString name, downloadLinksSolc.keys())
    {
        if(ui->comboBoxCompilerVersion->findText(name) < 0)
        {
            ui->comboBoxCompilerVersion->addItem(name);
            customizeComboBoxCompiler(ui->comboBoxCompilerVersion->count() - 1, true);
        }

    }
}

void EditContract::customizeComboBoxCompiler(int index, bool bDownload)
{
    if(bDownload)
    {
        ui->comboBoxCompilerVersion->setItemIcon(index, QIcon("://imgs/download.png"));
        ui->comboBoxCompilerVersion->setItemData(index,
                                                 QColor(Qt::gray), Qt::ForegroundRole);
    }
    else
    {
        ui->comboBoxCompilerVersion->setItemIcon(index, QIcon("://imgs/Check.png"));
        ui->comboBoxCompilerVersion->setItemData(index,
                                                 QColor(Qt::black), Qt::ForegroundRole);
    }

    ui->comboBoxCompilerVersion->setItemData(index,bDownload);

    if(index==ui->comboBoxCompilerVersion->currentIndex())
        slotChooseNewCompiler(index);
    ui->comboBoxCompilerVersion->model()->sort(0);
}


void EditContract::slotChooseNewCompiler(int index)
{

    bool bDownload = ui->comboBoxCompilerVersion->itemData(index).toBool();

    if(bDownload)
    {
        QPalette pal = ui->comboBoxCompilerVersion->palette();
        pal.setBrush(QPalette::ButtonText, Qt::gray);
        pal.setBrush(QPalette::Text, Qt::gray);
        ui->comboBoxCompilerVersion->setPalette(pal);
    }
    else
    {
        QPalette pal = ui->comboBoxCompilerVersion->palette();
        pal.setBrush(QPalette::ButtonText, Qt::black);
        pal.setBrush(QPalette::Text, Qt::black);
        ui->comboBoxCompilerVersion->setPalette(pal);
    }
}

#ifdef __linux__
void EditContract::slotProcDistribFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode)
    if(QProcess::NormalExit == exitStatus)
    {
        auto baData = process_linux_distrib->readAllStandardOutput();
        if(baData.contains("Ubuntu"))
        {
            bUbuntu = true;
            getDownloadLinksSolc();
        }
    }
}
#endif
void EditContract::slotOptimizationStateChanged(int state)
{
    if(Qt::Unchecked == state)
        ui->widgetRuns->setEnabled(false);
    else
        ui->widgetRuns->setEnabled(true);
}


void EditContract::slotErrWarningClicked(QListWidgetItem *item)
{
    if(item->data(defClickable_Item_Role).toBool())
    {
        int nLine = item->data(defY_Shift_Role).toInt();
        int nOffset = item->data(defX_Shift_Role).toInt();
        bool bTmp = item->data(defTmp_Role).toBool();
        QString absFilePath = item->data(defAbs_File_Path_Role).toString();
        QFileInfo info(absFilePath);
        openEditFile(info, bTmp, false);
        auto block = ui->codeEdit->document()->findBlockByNumber(nLine-1);
        QTextCursor cursor = ui->codeEdit->textCursor();
        cursor.setPosition(block.position() + nOffset);
        ui->codeEdit->setTextCursor(cursor);
    }
}

void EditContract::fillInCompilerVersions()
{
    QDir versionsDir(QCoreApplication::applicationDirPath() + "/" + strCompilePath);
    foreach(auto dir, versionsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        QDir subDir(versionsDir.absoluteFilePath(dir));
        QString exeName;
#ifdef __linux__
            exeName = "solc";
#endif
#ifdef _WIN32
            exeName = "solc.exe";
#endif
        if(subDir.entryList(QDir::Files).contains(exeName))
        {
            ui->comboBoxCompilerVersion->addItem(dir);
            //TODO add windows,...
            pathsSolc[dir] = QCoreApplication::applicationDirPath() + "/" + strCompilePath
                    + "/" + dir + "/" + exeName;
            customizeComboBoxCompiler(ui->comboBoxCompilerVersion->count() - 1, false);
        }
    }

    QString localSolcFileName = QCoreApplication::applicationDirPath() + "/" + strCompilePath + "/" + QString(defLocalSolcFile);
    QFile file(localSolcFileName);
    QJsonDocument documentOld;
    if(file.open(QIODevice::ReadOnly))
    {
        documentOld = QJsonDocument::fromJson(file.readAll());
        file.close();
    }
    QJsonArray array = documentOld.array();
    for(int i=0; i<array.size(); i++)
    {
        auto obj = array[i].toObject();
        QString path = obj[defPathCompiler].toString();
        QString version = obj[defVersionCompiler].toString();
        if(!path.isEmpty()
                &&
           !version.isEmpty())
        {
            ui->comboBoxCompilerVersion->addItem(version);
            pathsSolc[version] = path;
            customizeComboBoxCompiler(ui->comboBoxCompilerVersion->count() - 1, false);
        }
    }

    QString savedVersion = settings.value(defSolcVersion).toString();
    if(ui->comboBoxCompilerVersion->findText(savedVersion))
        ui->comboBoxCompilerVersion->setCurrentText(savedVersion);
}

bool EditContract::eventFilter(QObject *watched, QEvent *event)
{
    if(ui->listWidgetErrWarnings == qobject_cast<QListWidget*>(watched))
    {
        if(QEvent::Resize == event->type())
        {
            for(int i=0; i<ui->listWidgetErrWarnings->count(); i++)
            {
                auto index = ui->listWidgetErrWarnings->model()->index(i,0);
                auto item = ui->listWidgetErrWarnings->item(i);
                QLabel * label = qobject_cast<QLabel *>(ui->listWidgetErrWarnings->indexWidget(index));
                QFontMetrics fm(item->font());
                //TODO understand why 4*spacing (not 2)...
                int height = fm.boundingRect(QRect(0,0, ui->listWidgetErrWarnings->width() - 4*ui->listWidgetErrWarnings->spacing(), 100),
                                           Qt::TextWordWrap, label->text()).size().height();
                label->setMinimumSize(QSize(ui->listWidgetErrWarnings->width() - 4*ui->listWidgetErrWarnings->spacing(),
                                            height + defErrWarningsPadding));
                item->setSizeHint(QSize(ui->listWidgetErrWarnings->width() - 4*ui->listWidgetErrWarnings->spacing(),
                                        height + defErrWarningsPadding));
            }
        }
    }

    QListWidgetItem * item = reinterpret_cast<QListWidgetItem *>(watched->property("item").toLongLong());
    if(nullptr != item)
    {
        if(QEvent::MouseButtonPress == event->type())
        {
            item->setSelected(true);
            slotErrWarningClicked(item);
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void EditContract::slotBuildFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    ui->pushButtonBuild->setEnabled(true);
    ui->pushButtonAddCompiler->setEnabled(true);
    ui->comboBoxCompilerVersion->setEnabled(true);
    if(QProcess::CrashExit == exitStatus)
    {
        QMessageBox::critical(this, tr("Smart contract Build"),
                              tr("Crash code - ") + QString::number(exitCode));
        return;
    }
    //fill in comboBoxChooseDeploy
    {
        QDir deployDir(tmpDir->filePath("output"));
        foreach(auto fileInfo, deployDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot))
        {
            if(ui->comboBoxChooseDeploy->findText(fileInfo.baseName()) < 0)
                ui->comboBoxChooseDeploy->addItem(fileInfo.baseName());
        }
    }
    //fill in err_warnings and labelBuildStatus
    {
        QString errorBuild = QString(process_build->readAllStandardError());

        //remove ^--...---^, \n and others
        errorBuild.remove(QRegExp("\\n\\^\\s*\\-+\\s*\\^\\n"));
        QStringList err_warnings = errorBuild.split(";");

        //fill in listWidgetErrWarnings
        foreach(auto strWarning, err_warnings)
        {
            QStringList properties = strWarning.split(":", QString::SkipEmptyParts);
            if(properties.size() < 2)
                continue;
            QFileInfo info(properties[0]);
            strWarning.replace(properties[0], info.fileName());
            QListWidgetItem * item = new QListWidgetItem();
            ui->listWidgetErrWarnings->addItem(item);
            QLabel * label = new QLabel(ui->listWidgetErrWarnings);
            label->setWordWrap(true);
            label->setTextInteractionFlags(Qt::TextSelectableByMouse);
            label->setText(strWarning);
            label->setIndent(10);
            label->setAlignment(Qt::AlignLeft);
            label->setProperty("item", reinterpret_cast<qlonglong>(item));
            label->installEventFilter(this);
            //TODO understand why 4*spacing (not 2)...
            QFontMetrics fm(item->font());
            int height = fm.boundingRect(QRect(0,0, ui->listWidgetErrWarnings->width() - 4*ui->listWidgetErrWarnings->spacing(), 100),
                                       Qt::TextWordWrap, strWarning).size().height();
            label->setMinimumSize(QSize(ui->listWidgetErrWarnings->width() - 4*ui->listWidgetErrWarnings->spacing(),
                                        height + defErrWarningsPadding));
            item->setSizeHint(QSize(ui->listWidgetErrWarnings->width() - 4*ui->listWidgetErrWarnings->spacing(),
                                    height + defErrWarningsPadding));
            if(properties.size() >= 4)
            {
                item->setData(defX_Shift_Role, properties[2].simplified().remove(" ").toInt());
                item->setData(defY_Shift_Role, properties[1].simplified().remove(" ").toInt());
                item->setData(defAbs_File_Path_Role, properties[0]);
                item->setData(defClickable_Item_Role, true);
                item->setData(defTmp_Role, activeSolFileData().bTmp);
            }
            ui->listWidgetErrWarnings->setItemWidget(item,
                                                      label);

        }

        //add err_warnings to codeEdit
        bool bSuccess = true;
        QMap<QString, QMap<int, ErrWarningBuildData>> codeEditorWarnings;
        foreach(auto strWarning, err_warnings)
        {
            QString buildVersionPath = process_build->property(defBuildingSolcVersionPath).toString();
            if(strWarning.contains(buildVersionPath)
                    &&
               strWarning.toLower().contains("error"))
            {
                bSuccess = false;
                break;
            }
            ErrWarningBuildData data;
            QStringList properties = strWarning.split(":", QString::SkipEmptyParts);
            if(properties.size() < 4)
                continue;

            data.iY = properties[1].simplified().remove(" ").toInt();
            data.iX = properties[2].simplified().remove(" ").toInt();
            if(properties[3].contains("Error"))
            {
                data.type = ErrWarningBuildData::iError;
                bSuccess = false;
            }
            else
            {
                data.type = ErrWarningBuildData::iWarning;
            }
            QFileInfo info(properties[0]);
            data.message = strWarning.replace(properties[0], info.fileName());

            codeEditorWarnings[properties[0]][data.iY-1] = data;
        }
        ui->codeEdit->setErr_Warnings(codeEditorWarnings);
        ui->codeEdit->update();
        if(bSuccess)
        {
            ui->labelBuildStatus->setText("Successful Build");
            ui->labelBuildStatus->setProperty("success", true);
            ui->pushButtonDeploy->setEnabled(true);
        }
        else
        {
            ui->labelBuildStatus->setText("Failed Build");
            ui->labelBuildStatus->setProperty("success", false);
            ui->pushButtonDeploy->setEnabled(false);
        }
        ui->labelBuildStatus->style()->unpolish(ui->labelBuildStatus);
        ui->labelBuildStatus->style()->polish(ui->labelBuildStatus);

        qDebug() << errorBuild;
    }

    QByteArray dataBuild = process_build->readAllStandardOutput();
    qDebug() << dataBuild;
}

void EditContract::slotProgressDownSolc(qint64 bytesReceived, qint64 bytesTotal)
{
    if(bytesTotal != 0)
        ui->progressBarDownloadSolc->setValue((bytesReceived*100)/bytesTotal);
}

void EditContract::slotBuildProcError(QProcess::ProcessError error)
{
    ui->pushButtonBuild->setEnabled(true);
    ui->pushButtonAddCompiler->setEnabled(true);
    ui->comboBoxCompilerVersion->setEnabled(true);
    QMetaEnum metaEnum = QMetaEnum::fromType<QProcess::ProcessError>();
    QString strError = metaEnum.valueToKey(error);
    QMessageBox::critical(this, tr("Smart contract Build"),
                          "Crash error - " + strError);
}

void EditContract::slotSearchClicked()
{
    if(search_Wgt.isNull())
    {
        search_Wgt = new SearchWgt(this);
        search_Wgt->show();
        //signals-slots connects
        {
            //mark
            connect(search_Wgt.data(), &SearchWgt::sigMark,
                    ui->codeEdit, &CodeEditor::slotSearchMark);

            //find
            connect(search_Wgt.data(), &SearchWgt::sigFindAllCurrentFile,
                    ui->codeEdit, &CodeEditor::slotFindAllCurrentFile);
            connect(search_Wgt.data(), &SearchWgt::sigFindAllAllFiles,
                    ui->codeEdit, &CodeEditor::slotFindAllAllFiles);
            connect(search_Wgt.data(), &SearchWgt::sigFindAllCurProject,
                    ui->codeEdit, &CodeEditor::slotFindAllCurProject);
            connect(search_Wgt.data(), &SearchWgt::sigFindAllCurrentFile,
                    this, [this](){ui->wgtFindResults->show();});
            connect(search_Wgt.data(), &SearchWgt::sigFindAllAllFiles,
                    this, [this](){ui->wgtFindResults->show();});
            connect(search_Wgt.data(), &SearchWgt::sigFindAllCurProject,
                    this, [this](){ui->wgtFindResults->show();});
            connect(search_Wgt.data(), &SearchWgt::sigFindNext,
                    ui->codeEdit, &CodeEditor::slotFindNext);
            connect(search_Wgt.data(), &SearchWgt::sigFindPrev,
                    ui->codeEdit, &CodeEditor::slotFindPrev);

            //replace
            connect(search_Wgt.data(), &SearchWgt::sigReplace,
                    ui->codeEdit, &CodeEditor::slotReplace);
            connect(search_Wgt.data(), &SearchWgt::sigReplaceAllCurrent,
                    ui->codeEdit, &CodeEditor::slotReplaceAllCurrent);
            connect(search_Wgt.data(), &SearchWgt::sigReplaceAllCurProject,
                    ui->codeEdit, &CodeEditor::slotReplaceAllCurProject);
            connect(search_Wgt.data(), &SearchWgt::sigReplaceAllAll,
                    ui->codeEdit, &CodeEditor::slotReplaceAllAll);
        }
    }
    else
            search_Wgt->activateWindow();
}

void EditContract::slotBuildClicked()
{
    bool bDownload = ui->comboBoxCompilerVersion->itemData(
                ui->comboBoxCompilerVersion->currentIndex()).toBool();
    if(!bDownload)
        startBuild();
    else
    {
        ui->comboBoxCompilerVersion->setEnabled(false);
        ui->pushButtonBuild->setEnabled(false);
        ui->pushButtonAddCompiler->setEnabled(false);
        QString downloadLink = downloadLinksSolc[ui->comboBoxCompilerVersion->currentText()];
        auto reply = nam->get(QNetworkRequest(QUrl(downloadLink)));
        reply->setProperty("Version", ui->comboBoxCompilerVersion->currentText());
        connect(reply, &QNetworkReply::finished,
                this, &EditContract::slotDownSolcFinished);
    }
}

void EditContract::slotDownSolcFinished()
{
    auto reply = qobject_cast<QNetworkReply *>(sender());

    if(reply->error() != QNetworkReply::NoError)
    {
        ui->comboBoxCompilerVersion->setEnabled(true);
        ui->pushButtonBuild->setEnabled(true);
        ui->progressBarDownloadSolc->setEnabled(false);
        QString data = QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(reply->error());
        ui->labelBuildStatus->setText(tr("Could not download compiler. Error - ") + data);
        ui->labelBuildStatus->setProperty("success", false);
        ui->labelBuildStatus->style()->unpolish(ui->labelBuildStatus);
        ui->labelBuildStatus->style()->polish(ui->labelBuildStatus);
        return;
    }

    //redirect
    {
        QVariant redirectUrl =
                 reply->attribute(QNetworkRequest::RedirectionTargetAttribute);

        if(!redirectUrl.toUrl().isEmpty())
        {
            ui->progressBarDownloadSolc->show();
            ui->progressBarDownloadSolc->setValue(0);
            auto replyNew = nam->get(QNetworkRequest(redirectUrl.toUrl()));
            replyNew->setProperty("Version", reply->property("Version"));
            connect(replyNew, &QNetworkReply::finished,
                    this, &EditContract::slotDownSolcFinished);
            connect(replyNew, &QNetworkReply::downloadProgress,
                    this, &EditContract::slotProgressDownSolc);
            return;
        }
    }

    ui->progressBarDownloadSolc->hide();

    QByteArray dataReply = reply->readAll();
    QDir versionsDir(QCoreApplication::applicationDirPath() + "/" + strCompilePath);
    QString version = reply->property("Version").toString();
    versionsDir.mkdir(version);
    bool bSuccess = false;
    //TODO add windows,...
    QFile file_compiler(versionsDir.absoluteFilePath(version + "/solc.zip"));
    if(file_compiler.open(QIODevice::WriteOnly))
    {
        file_compiler.write(dataReply);
        file_compiler.close();
        auto list = JlCompress::extractDir(&file_compiler, versionsDir.absoluteFilePath(version + "/output"));
//variant with sudo apt install libssl-dev unzip
#if 0
        QProcess proc;
        proc.setWorkingDirectory(versionsDir.absoluteFilePath(version));
#ifdef __linux__
        if(bUbuntu)
        {
            proc.start("unzip", QStringList() << "solc.zip" << "-d" << "output");
        }
#endif
#ifdef _WIN32
        proc.start("unzip.exe", QStringList() << "solc.zip" << "-d" << "output");
#endif
        proc.waitForFinished();*/

        if(QProcess::NormalExit == proc.exitStatus())
#endif
        if(!list.isEmpty())
        {
            QFile::remove(versionsDir.absoluteFilePath(version + "/solc.zip"));
            QDir output(versionsDir.absoluteFilePath(version + "/output"));
            auto files = output.entryList(QDir::Files);
            bSuccess = true;
            foreach(auto file, files)
            {
                //if(file.contains("sol"))
                bSuccess &= QFile::copy(versionsDir.absoluteFilePath(version + "/output/" + file),
                            versionsDir.absoluteFilePath(version + "/" + file));
            }
            output.removeRecursively();
        }
    }


    if(bSuccess)
    {
#ifdef __linux__
        pathsSolc[version]
                = versionsDir.absoluteFilePath(version + "/solc");
#endif
#ifdef _WIN32
        pathsSolc[version]
                = versionsDir.absoluteFilePath(version + "/solc.exe");
#endif
        customizeComboBoxCompiler(ui->comboBoxCompilerVersion->currentIndex(), false);
        startBuild();
    }
    else
    {
        ui->labelBuildStatus->setText(tr("Could not create new compiler"));
        ui->labelBuildStatus->setProperty("success", false);
        ui->labelBuildStatus->style()->unpolish(ui->labelBuildStatus);
        ui->labelBuildStatus->style()->polish(ui->labelBuildStatus);
    }
}


void EditContract::startBuild()
{
    settings.setValue(defSolcVersion, ui->comboBoxCompilerVersion->currentText());
    ui->pushButtonBuild->setEnabled(false);
    ui->comboBoxChooseDeploy->clear();
    ui->listWidgetErrWarnings->clear();

    QFileInfo infoActiveSol = activeSolFileData().fileInfo;
    if(activeSolFileData().bTmp)
        infoActiveSol = QFileInfo(tmpDir->filePath("tmp.sol"));

    QString outputPath = infoActiveSol.absolutePath() + "/output";
    QDir deployDir(outputPath);
    deployDir.removeRecursively();
    QDir(infoActiveSol.path()).mkpath("output");
    QString activeSolPath = infoActiveSol.absoluteFilePath();

    if(activeSolFileData().bTmp)
    {
        QFile file_tmp(activeSolPath);
        if(!file_tmp.open(QIODevice::WriteOnly))
        {
            QMessageBox::critical(this, tr("Smart contract Build"),
                                  tr("Can not create tmp file to build"));
            return;
        }
        file_tmp.write(ui->codeEdit->document()->toPlainText().toLocal8Bit());
        file_tmp.close();
    }
    QString params = " --bin --abi --overwrite ";
    //QString params;
    if(ui->checkBoxOptimization->isChecked())
    {
        int nRuns = ui->spinBoxRuns->value();
        params = " --optimize-runs " + QString::number(nRuns) + params;
    }
    if(!activeSolcImportPathes.isEmpty())
    {
        params += " --allow-paths ";
        for(int i=0; i<activeSolcImportPathes.size(); i++)
        {
            params += activeSolcImportPathes[i] + ",";
        }
        params.remove(params.size() - 1, 1);
        params += " ";
    }

    QString version = ui->comboBoxCompilerVersion->currentText();
    qDebug() << pathsSolc[version];
    //process_build->setWorkingDirectory(infoActiveSol.path());
    QString strProcExec = pathsSolc[version]
            + params + activeSolPath
            + " -o " + outputPath;
    qDebug() << strProcExec;
    process_build->start(strProcExec);
    process_build->setProperty(defBuildingSolcPath,infoActiveSol.path());
    process_build->setProperty(defBuildingSolcVersionPath,pathsSolc[version]);
}

EditContract::~EditContract()
{
    tmpDir->remove();
    delete ui;
}
