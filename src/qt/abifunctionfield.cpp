#include "abifunctionfield.h"
#include "abiparamsfield.h"
#include "contractabi.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QStringListModel>
#include <QPainter>

#include <iostream>
ABIFunctionField::ABIFunctionField(FunctionType type, QWidget *parent) :
    QWidget(parent),
    m_contractABI(0),
    m_func(new QWidget(this)),
    m_comboBoxFunc(new QComboBox(this)),
    m_paramsField(new QStackedWidget(this)),
    m_functionType(type)
{
    // Setup layouts
    m_comboBoxFunc->setFixedWidth(370);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *topLayout = new QHBoxLayout(m_func);
    topLayout->setSpacing(10);
    topLayout->setContentsMargins(0, 0, 0, 0);

    m_labelFunction = new QLabel(tr("Function"));
    m_labelFunction->setFixedWidth(160);
    topLayout->addWidget(m_labelFunction);

    topLayout->addWidget(m_comboBoxFunc);
    topLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Fixed));

    m_func->setLayout(topLayout);
    mainLayout->addWidget(m_func);
    mainLayout->addWidget(m_paramsField);
    mainLayout->addSpacerItem(new QSpacerItem(20, 40, QSizePolicy::Fixed, QSizePolicy::Expanding));
    connect(m_comboBoxFunc, SIGNAL(currentIndexChanged(int)), m_paramsField, SLOT(setCurrentIndex(int)));
    connect(m_paramsField, SIGNAL(currentChanged(int)), this, SLOT(on_currentIndexChanged()));

    m_func->setVisible(false);
}

void ABIFunctionField::updateABIFunctionField()
{
    // Clear the content
    clear();

    if(m_contractABI != NULL)
    {
        // Populate the control with functions
        std::vector<FunctionABI> functions = m_contractABI->functions;
        QStringList functionList;
        QStringListModel *functionModel = new QStringListModel(this);
        bool bFieldCreate = m_functionType == Create;
        bool bFieldCall = m_functionType == Call;
        bool bFieldSendTo = m_functionType == SendTo;
        bool bFieldFunc = bFieldCall || bFieldSendTo;
        for (int func = 0; func < (int)functions.size(); ++func)
        {
            const FunctionABI &function = functions[func];
            bool bTypeConstructor = function.type == "constructor";
            bool bTypeEvent = function.type == "event";
            bool bTypeDefault = function.type == "default";
            bool bIsConstant = function.constant;
            if((bFieldCreate && !bTypeConstructor) ||
                    (bFieldFunc && bTypeConstructor) ||
                    (bFieldFunc && bTypeEvent) ||
                    (bFieldCall && !bIsConstant && !bTypeDefault) ||
                    (bFieldSendTo && bIsConstant && !bTypeDefault))
            {
                continue;
            }

            ABIParamsField *abiParamsField = new ABIParamsField(this);
            abiParamsField->updateParamsField(function);

            m_paramsField->addWidget(abiParamsField);
            QString funcName = QString::fromStdString(function.name);
            QString funcSelector = QString::fromStdString(function.selector());
            functionList.append(QString(funcName + "(" + funcSelector + ")"));

            m_abiFunctionList.append(func);
        }
        functionModel->setStringList(functionList);
        m_comboBoxFunc->setModel(functionModel);

        if(bFieldFunc)
        {
            bool visible = m_abiFunctionList.size() > 0;
            m_func->setVisible(visible);
        }
        on_currentIndexChanged();
    }
}

void ABIFunctionField::clear()
{
    m_comboBoxFunc->clear();
    m_abiFunctionList.clear();
    for(int i = m_paramsField->count() - 1; i >= 0; i--)
    {
        QWidget* widget = m_paramsField->widget(i);
        m_paramsField->removeWidget(widget);
        widget->deleteLater();
    }
}

void ABIFunctionField::paintEvent(QPaintEvent *)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ABIFunctionField::setContractABI(ContractABI *contractABI)
{
    m_contractABI = contractABI;
    updateABIFunctionField();
}

QStringList ABIFunctionField::getParamValue(int paramID)
{
    if(m_paramsField->currentWidget() == 0)
        return QStringList();

    return ((ABIParamsField*)m_paramsField->currentWidget())->getParamValue(paramID);
}

QList<QStringList> ABIFunctionField::getParamsValues()
{
    if(m_paramsField->currentWidget() == 0)
        return QList<QStringList>();

    return ((ABIParamsField*)m_paramsField->currentWidget())->getParamsValues();
}

std::vector<std::vector<std::string>> ABIFunctionField::getValuesVector()
{
    QList<QStringList> qlist = getParamsValues();
    std::vector<std::vector<std::string>> result;
    for (int i=0; i<qlist.size(); i++)
    {
        std::vector<std::string> itemParam;
        QStringList qlistVlaues = qlist[i];
        for(int j=0; j<qlistVlaues.size(); j++)
        {
            itemParam.push_back(qlistVlaues.at(j).toUtf8().data());
        }
        result.push_back(itemParam);
    }
    return result;
}

int ABIFunctionField::getSelectedFunction() const
{
    // Get the currently selected function
    int currentFunc = m_comboBoxFunc->currentIndex();
    if(currentFunc == -1)
        return -1;

    return m_abiFunctionList[currentFunc];
}

bool ABIFunctionField::isValid()
{
    if(m_paramsField->currentWidget() == 0)
        return true;

    return ((ABIParamsField*)m_paramsField->currentWidget())->isValid();
}

void ABIFunctionField::on_currentIndexChanged()
{
    for (int i = 0; i < m_paramsField->count (); ++i)
    {
        QSizePolicy::Policy policy = QSizePolicy::Ignored;
        if (i == m_paramsField->currentIndex())
            policy = QSizePolicy::Expanding;

        QWidget* pPage = m_paramsField->widget(i);
        pPage->setSizePolicy(policy, policy);
    }
    Q_EMIT(functionChanged());
}

