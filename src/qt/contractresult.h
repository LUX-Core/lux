#ifndef CONTRACTRESULT_H
#define CONTRACTRESULT_H

#include <QStackedWidget>
#include <QString>

class FunctionABI;
class WalletModel;
class ContractTableModel;

namespace Ui {
class ContractResult;
}

class ContractResult : public QStackedWidget
{
    Q_OBJECT

public:
    enum ContractTxType{
        CreateResult,
        SendToResult,
        CallResult
    };
    explicit ContractResult(QWidget *parent = 0);
    ~ContractResult();
    void setResultData(QVariant result, FunctionABI function, QList<QStringList> paramValues, ContractTxType type);
    void setModel(WalletModel *model) { walletModel = model; };
    void setABIText(const QString& abi) { abiText = abi; };

public slots:
    void on_toContractBookStateChanged();
    void on_okButtonClicked();

private:
    Ui::ContractResult *ui;
    WalletModel *walletModel;
    void setParamsData(FunctionABI function, QList<QStringList> paramValues);
    void updateCreateResult(QVariant result);
    void updateSendToResult(QVariant result);
    void updateCallResult(QVariant result, FunctionABI function, QList<QStringList> paramValues);
    QString abiText;
};

#endif // CONTRACTRESULT_H
