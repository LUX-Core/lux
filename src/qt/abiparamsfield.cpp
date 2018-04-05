#include "abiparamsfield.h"
#include "abiparam.h"
#include "platformstyle.h"

#include "QStringList"

ABIParamsField::ABIParamsField(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    m_mainLayout(new QVBoxLayout(this))
{
    m_platfromStyle = platformStyle;
    m_mainLayout->setSpacing(10);
    m_mainLayout->setContentsMargins(0,0,30,0);
    this->setLayout(m_mainLayout);
}

void ABIParamsField::updateParamsField(const FunctionABI &function)
{
    // Add function parameters
    m_listParams.clear();
    int paramId = 0;
    for(std::vector<ParameterABI>::const_iterator param = function.inputs.begin(); param != function.inputs.end(); ++param)
    {
        ABIParam *paramFiled = new ABIParam(m_platfromStyle, paramId, *param);
        m_listParams.append(paramFiled);
        m_mainLayout->addWidget(paramFiled);

        paramId++;
    }
    m_mainLayout->addSpacerItem(new QSpacerItem(20, 40, QSizePolicy::Fixed, QSizePolicy::Expanding));
}

QStringList ABIParamsField::getParamValue(int paramID)
{
    // Get parameter value
    return m_listParams[paramID]->getValue();
}

QList<QStringList> ABIParamsField::getParamsValues()
{
    // Get parameters values
    QList<QStringList> resultList;
    for(int i = 0; i < m_listParams.count(); ++i){
        resultList.append(m_listParams[i]->getValue());
    }
    return resultList;
}

bool ABIParamsField::isValid()
{
    bool isValid = true;
    for(int i = 0; i < m_listParams.count(); ++i){
        if(!m_listParams[i]->isValid())
            isValid = false;
    }
    return isValid;
}
