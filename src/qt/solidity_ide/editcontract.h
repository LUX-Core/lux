#ifndef CREATECONTRACT_H
#define CREATECONTRACT_H

#include <QWidget>
#include <QPointer>
#include <QProcess>
#include <QMap>
#include <QStringList>
#include <QNetworkReply>
#include <QSettings>
#include <QFileInfo>


#include "allopenfilesmodel.h"
#include "searchwgt.h"

class QTemporaryDir;
class QListWidgetItem;
class QNetworkAccessManager;
class QSortFilterProxyModel;
class QTreeWidgetItem;
class ScWizardWgt;

namespace Ui {
class EditContract;
}

class EditContract : public QWidget
{
    Q_OBJECT
public:
    explicit EditContract(QWidget *parent = 0);
    ~EditContract();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event);
private:
    enum activeProjectFolders {_ActiveSol, _UpdatesFolder};
    Ui::EditContract *ui;
#ifdef __linux__
    bool bUbuntu {false};
#endif
    QPointer<SearchWgt> search_Wgt;
    QPointer<ScWizardWgt> sc_wizard_Wgt;
    QPointer<QProcess> process_build;
    QPointer<QProcess> process_linux_distrib;
    QPointer<QSortFilterProxyModel> allFilesModel;
    QTemporaryDir * tmpDir;
    QNetworkAccessManager * nam;
    QSettings settings;
    //test
    const QString strCompilePath{"SolCompilers"};
    void fillInCompilerVersions();
    //get download links of solc
    //in https://github.com/ethereum/solidity/releases ubuntu and windows releases
    void getDownloadLinksSolc();
    QMap<QString, QString> downloadLinksSolc; //<version, downloadLink>
    QMap<QString, QString> pathsSolc; //<version, path>
    QStringList activeSolcImportPathes;
    void customizeComboBoxCompiler(int index, bool bDownload);
    void startBuild();
    void openEditFile(const QFileInfo &  info, bool bTmp, bool bActiveSol);
    void fillInImports(QString absFilePath);
    QStringList parseImports(QString absFilePath);
    EditFileData editingFileData();
    EditFileData activeSolFileData();
    //index in proxy model
    QModelIndex indexAllOpenFiles(QString absFilePath);
    bool bNewTextOpenFile {false};
    void unSaveChangesEditFile(bool bUnSave);
    int NumTmpFiles {0};
private slots:
    void slotListAllOpenClickFile(const QModelIndex &index);
    void slotTreeCurProjClickFile(QTreeWidgetItem *item);
    void slotAddSolcManually();
    void slotDownSolcFinished();
    void slotProgressDownSolc(qint64 bytesReceived,
                              qint64 bytesTotal);
    void slotDownLinksSolcFinished();
#ifdef __linux__
    void slotProcDistribFinished(int exitCode, QProcess::ExitStatus exitStatus);
#endif
    void slotSearchClicked();
    void slotBuildClicked();
    void slotBuildFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void slotBuildProcError(QProcess::ProcessError error);
    void slotErrWarningClicked(QListWidgetItem *item);
    void slotOptimizationStateChanged(int state);
    void slotChooseNewCompiler(int index);
    void slotClickOpenFile();
    void slotSolcCodeChanged(int position, int charsRemoved, int charsAdded);
    void slotSaveAllFilesClick();
    void slotSaveFileClick();
    void slotSaveFileAsClick();
    void slotNewFileClick();
    void slotFindResultChoosed(QString nameFile);
    void slotWizardSCClick();
};

#endif // CREATECONTRACT_H
