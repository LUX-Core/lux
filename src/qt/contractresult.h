#ifndef CONTRACTRESULT_H
#define CONTRACTRESULT_H

#include <QStackedWidget>

class FunctionABI;

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

private:
    Ui::ContractResult *ui;
    void setParamsData(FunctionABI function, QList<QStringList> paramValues);
    void updateCreateResult(QVariant result);
    void updateSendToResult(QVariant result);
    void updateCallResult(QVariant result, FunctionABI function, QList<QStringList> paramValues);
};

#endif // CONTRACTRESULT_H
