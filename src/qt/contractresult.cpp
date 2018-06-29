#include "contractresult.h"
#include "ui_contractresult.h"
#include "guiconstants.h"
#include "contractabi.h"
#include "contracttablemodel.h"
#include "walletmodel.h"

#include <QMessageBox>
#include <QCheckBox>

ContractResult::ContractResult(QWidget *parent) :
    QStackedWidget(parent),
    ui(new Ui::ContractResult)
{
    ui->setupUi(this);

    connect(ui->toContractBook, SIGNAL(stateChanged(int)), SLOT(on_toContractBookStateChanged()));
    connect(ui->okButton, SIGNAL(clicked()), SLOT(on_okButtonClicked()));
}

ContractResult::~ContractResult()
{
    delete ui;
}

void ContractResult::on_toContractBookStateChanged() {
    ui->contractLabel->setEnabled(ui->toContractBook->isChecked());
}

void ContractResult::on_okButtonClicked() {
    if (!ui->toContractBook->isChecked()) {
        close();
        return;
    }

    walletModel->getContractTableModel()->addRow(ui->contractLabel->text(), ui->lineEditContractAddress->text(), abiText);
    close();
}

void ContractResult::setResultData(QVariant result, FunctionABI function, QList<QStringList> paramValues, ContractTxType type)
{
    switch (type) {
    case CreateResult:
        updateCreateResult(result);
        setCurrentWidget(ui->pageCreateOrSendToResult);
        ui->groupContractBookOptional->setVisible(true);
        break;

    case SendToResult:
        updateSendToResult(result);
        setCurrentWidget(ui->pageCreateOrSendToResult);
        ui->groupContractBookOptional->setVisible(false);
        break;

    case CallResult:
        updateCallResult(result, function, paramValues);
        setCurrentWidget(ui->pageCallResult);
        break;

    default:
        break;
    }
}

void ContractResult::setParamsData(FunctionABI function, QList<QStringList> paramValues)
{
    // Remove previous widget from scroll area
    QWidget *scrollWidget = ui->scrollAreaParams->widget();
    if(scrollWidget)
        scrollWidget->deleteLater();

    // Don't show empty list
    if(function.inputs.size() == 0)
    {
        ui->scrollAreaParams->setVisible(false);
        return;
    }

    QWidget *widgetParams = new QWidget(this);
    widgetParams->setObjectName("scrollAreaWidgetContents");

    QVBoxLayout *mainLayout = new QVBoxLayout(widgetParams);
    mainLayout->setSpacing(6);
    mainLayout->setContentsMargins(0,0,30,0);

    // Add rows with params and values sent
    int i = 0;
    for(std::vector<ParameterABI>::const_iterator param = function.inputs.begin(); param != function.inputs.end(); ++param)
    {
        QHBoxLayout *hLayout = new QHBoxLayout();
        hLayout->setSpacing(10);
        hLayout->setContentsMargins(0,0,0,0);
        QVBoxLayout *vNameLayout = new QVBoxLayout();
        vNameLayout->setSpacing(3);
        vNameLayout->setContentsMargins(0,0,0,0);
        QVBoxLayout *paramValuesLayout = new QVBoxLayout();
        paramValuesLayout->setSpacing(3);
        paramValuesLayout->setContentsMargins(0,0,0,0);

        QLabel *paramName = new QLabel(this);
        paramName->setFixedWidth(160);
        paramName->setFixedHeight(19);
        QFontMetrics metrix(paramName->font());
        int width = paramName->width() + 10;
        QString text(QString("%2 <b>%1").arg(QString::fromStdString(param->name)).arg(QString::fromStdString(param->type)));
        QString clippedText = metrix.elidedText(text, Qt::ElideRight, width);
        paramName->setText(clippedText);
        paramName->setToolTip(QString("%2 %1").arg(QString::fromStdString(param->name)).arg(QString::fromStdString(param->type)));

        vNameLayout->addWidget(paramName);
        hLayout->addLayout(vNameLayout);
        QStringList listValues = paramValues[i];
        if(listValues.size() > 0)
        {
            int spacerSize = 0;
            for(int j = 0; j < listValues.count(); j++)
            {
                QLineEdit *paramValue = new QLineEdit(this);
                paramValue->setReadOnly(true);
                paramValue->setText(listValues[j]);
                paramValuesLayout->addWidget(paramValue);
                if(j > 0)
                    spacerSize += 30; // Line edit height + spacing
            }
            if(spacerSize > 0)
                vNameLayout->addSpacerItem(new QSpacerItem(20, spacerSize, QSizePolicy::Fixed, QSizePolicy::Fixed));

        hLayout->addLayout(paramValuesLayout);
        }
        else
        {
            hLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Fixed));
        }

        mainLayout->addLayout(hLayout);
        i++;
    }
    widgetParams->setLayout(mainLayout);
    widgetParams->adjustSize();
    if(widgetParams->sizeHint().height() < 70)
        ui->scrollAreaParams->setMaximumHeight(widgetParams->sizeHint().height() + 2);
    else
        ui->scrollAreaParams->setMaximumHeight(140);
    ui->scrollAreaParams->setWidget(widgetParams);
    ui->scrollAreaParams->setVisible(true);
}

void ContractResult::updateCreateResult(QVariant result)
{
    ui->lineEditContractAddress->setVisible(true);
    ui->labelContractAddress->setVisible(true);

    QVariantMap variantMap = result.toMap();

    ui->lineEditTxID->setText(variantMap.value("txid").toString());
    ui->lineEditSenderAddress->setText(variantMap.value("sender").toString());
    ui->lineEditHash160->setText(variantMap.value("hash160").toString());
    ui->lineEditContractAddress->setText(variantMap.value("address").toString());
}

void ContractResult::updateSendToResult(QVariant result)
{
    ui->lineEditContractAddress->setVisible(false);
    ui->labelContractAddress->setVisible(false);

    QVariantMap variantMap = result.toMap();

    ui->lineEditTxID->setText(variantMap.value("txid").toString());
    ui->lineEditSenderAddress->setText(variantMap.value("sender").toString());
    ui->lineEditHash160->setText(variantMap.value("hash160").toString());
}

void ContractResult::updateCallResult(QVariant result, FunctionABI function, QList<QStringList> paramValues)
{
    QVariantMap variantMap = result.toMap();
    QVariantMap executionResultMap = variantMap.value("executionResult").toMap();

    ui->lineEditCallContractAddress->setText(variantMap.value("address").toString());
    ui->lineEditFunction->setText(QString::fromStdString(function.name) + "()");

    setParamsData(function, paramValues);

    ui->lineEditCallSenderAddress->setText(variantMap.value("sender").toString());
    std::string rawData = executionResultMap.value("output").toString().toStdString();
    std::vector<std::vector<std::string>> values;
    std::vector<ParameterABI::ErrorType> errors;
    if(function.abiOut(rawData, values, errors))
    {
        // Remove previous widget from scroll area
        QWidget *scrollWidget = ui->scrollAreaResult->widget();
        if(scrollWidget)
            scrollWidget->deleteLater();

        if(values.size() > 0)
        {
            QWidget *widgetResults = new QWidget(this);
            widgetResults->setObjectName("scrollAreaWidgetContents");

            QVBoxLayout *mainLayout = new QVBoxLayout(widgetResults);
            mainLayout->setSpacing(6);
            mainLayout->setContentsMargins(0,6,0,6);
            widgetResults->setLayout(mainLayout);

            for(size_t i = 0; i < values.size(); i++)
            {
                QHBoxLayout *hLayout = new QHBoxLayout();
                hLayout->setSpacing(10);
                hLayout->setContentsMargins(0,0,0,0);
                QVBoxLayout *vNameLayout = new QVBoxLayout();
                vNameLayout->setSpacing(3);
                vNameLayout->setContentsMargins(0,0,0,0);
                QVBoxLayout *paramValuesLayout = new QVBoxLayout();
                paramValuesLayout->setSpacing(3);
                paramValuesLayout->setContentsMargins(0,0,0,0);

                QLabel *resultName = new QLabel(this);
                resultName->setFixedWidth(160);
                resultName->setFixedHeight(19);
                QFontMetrics metrix(resultName->font());
                int width = resultName->width() + 10;
                QString text(QString("%2 <b>%1").arg(QString::fromStdString(function.outputs[i].name)).arg(QString::fromStdString(function.outputs[i].type)));
                QString clippedText = metrix.elidedText(text, Qt::ElideRight, width);
                resultName->setText(clippedText);
                resultName->setToolTip(QString("%2 %1").arg(QString::fromStdString(function.outputs[i].name)).arg(QString::fromStdString(function.outputs[i].type)));

                vNameLayout->addWidget(resultName);
                std::vector<std::string> listValues = values[i];
                hLayout->addLayout(vNameLayout);
                if(listValues.size() > 0)
                {
                    int spacerSize = 0;
                    for(size_t j = 0; j < listValues.size(); j++)
                    {
                        QLineEdit *resultValue = new QLineEdit(this);
                        resultValue->setReadOnly(true);
                        resultValue->setText(QString::fromStdString(listValues[j]));
                        paramValuesLayout->addWidget(resultValue);
                        if(j > 0)
                            spacerSize += 30; // Line edit height + spacing
                    }
                    if(spacerSize > 0)
                        vNameLayout->addSpacerItem(new QSpacerItem(20, spacerSize, QSizePolicy::Fixed, QSizePolicy::Fixed));
                    hLayout->addLayout(paramValuesLayout);
                }
                else
                {
                    hLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Fixed));
                }
                mainLayout->addLayout(hLayout);
            }
            widgetResults->adjustSize();
            if(widgetResults->sizeHint().height() < 70)
            {
                ui->scrollAreaResult->setMaximumHeight(widgetResults->sizeHint().height() + 2);
            }
            else
            {
                ui->scrollAreaResult->setMaximumHeight(140);
            }
            ui->scrollAreaResult->setWidget(widgetResults);
            ui->groupBoxResult->setVisible(true);
        }
        else
        {
            ui->groupBoxResult->setVisible(false);
        }
    }
    else
    {
        QString errorMessage;
        errorMessage = function.errorMessage(errors, false);
        QMessageBox::warning(this, tr("Create contract"), errorMessage);
    }
}
