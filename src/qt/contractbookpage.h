#ifndef CONTRACTBOOKPAGE_H
#define CONTRACTBOOKPAGE_H

#include <QDialog>

class ContractTableModel;

namespace Ui {
class ContractBookPage;
}
QT_BEGIN_NAMESPACE
class QMenu;
class QSortFilterProxyModel;
QT_END_NAMESPACE

class ContractBookPage : public QDialog
{
    Q_OBJECT

public:
    explicit ContractBookPage(QWidget *parent = 0);
    ~ContractBookPage();

    enum ColumnWidths {
            LABEL_COLUMN_WIDTH = 180,
            ADDRESS_COLUMN_WIDTH = 380,
        };

    void setModel(ContractTableModel *model);
    const QString &getAddressValue() const { return addressValue; }
    const QString &getABIValue() const { return ABIValue; }


public Q_SLOTS:
    void done(int retval);

private:
    Ui::ContractBookPage *ui;
    ContractTableModel *model;
    QString addressValue;
    QString ABIValue;
    QSortFilterProxyModel *proxyModel;
    QMenu *contextMenu;
    QString newContractInfoToSelect;

private Q_SLOTS:
    /** Delete currently selected contract info entry */
    void on_deleteContractInfo_clicked();
    /** Create a new contract info */
    void on_newContractInfo_clicked();
    /** Copy address of currently selected contract info entry to clipboard */
    void on_copyAddress_clicked();
    /** Copy label of currently selected contract info entry to clipboard (no button) */
    void onCopyNameAction();
    /** Copy ABI of currently selected contract info entry to clipboard (no button) */
    void onCopyABIAction();
    /** Edit currently selected contract info entry (no button) */
    void onEditAction();
    /** Export button clicked */
    void on_exportButton_clicked();

    /** Set button states based on selection */
    void selectionChanged();
    /** Spawn contextual menu (right mouse menu) for contract info book entry */
    void contextualMenu(const QPoint &point);
    /** New entry/entries were added to contract info table */
    void selectNewContractInfo(const QModelIndex &parent, int begin, int /*end*/);

};

#endif // CONTRACTBOOKPAGE_H
