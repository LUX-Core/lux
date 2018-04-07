#include "abiparam.h"
#include "contractabi.h"
#include "abiparamitem.h"

#include <QHBoxLayout>
#include <QRegularExpressionValidator>

ABIParam::ABIParam( int ID, const ParameterABI &param, QWidget *parent) :
    QWidget(parent),
    m_ParamID(ID),
    m_paramName(0),
    m_mainLayout(0),
    m_paramItemsLayout(0),
    m_param(param),
    m_vSpacer(0),
    m_hSpacer(0)
{
    m_paramName = new QLabel(this);
    m_mainLayout = new QHBoxLayout(this);
    m_paramItemsLayout = new QVBoxLayout();
    m_vSpacer = new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_hSpacer = new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_mainLayout->setSpacing(10);
    m_mainLayout->setContentsMargins(0,0,0,0);

    m_paramItemsLayout->setSpacing(3);
    m_paramItemsLayout->setContentsMargins(0,0,0,0);

    m_paramName->setToolTip(tr("%1 %2").arg(QString::fromStdString(param.type)).arg(QString::fromStdString(param.name)));
    m_paramName->setFixedWidth(160);
    m_paramName->setFixedHeight(27);

    QFontMetrics metrix(m_paramName->font());
    int width = m_paramName->width() + 10;
    QString text(QString("%2 <b>%1").arg(QString::fromStdString(param.name)).arg(QString::fromStdString(param.type)));
    QString clippedText = metrix.elidedText(text, Qt::ElideRight, width);
    m_paramName->setText(clippedText);

    QVBoxLayout *vLayout = new QVBoxLayout();
    vLayout->addWidget(m_paramName);
    vLayout->addSpacerItem(m_vSpacer);
    m_mainLayout->addLayout(vLayout);

    if(param.decodeType().isList())
    {
        if(param.decodeType().isDynamic())
        {
            addNewParamItem(0);
        }
        else
        {
            for(size_t i = 0; i < param.decodeType().length(); i++)
            {
                ABIParamItem *m_paramValue = new ABIParamItem(m_param, this);
                m_paramValue->setFixed(true);
                m_paramItemsLayout->addWidget(m_paramValue);
                m_listParamItems.append(m_paramValue);
            }
            if(param.decodeType().length() > 1)
            {
                m_vSpacer->changeSize(20, 40, QSizePolicy::Fixed, QSizePolicy::Expanding);
            }
        }
    }
    else
    {
        ABIParamItem *m_paramValue = new ABIParamItem(m_param, this);
        m_paramValue->setFixed(true);
        m_paramItemsLayout->addWidget(m_paramValue);
        m_listParamItems.append(m_paramValue);
    }
    m_mainLayout->addLayout(m_paramItemsLayout);
    m_mainLayout->addSpacerItem(m_hSpacer);
    setLayout(m_mainLayout);
}

QStringList ABIParam::getValue()
{
    QStringList valuesList;
    for(int i = 0; i < m_listParamItems.count(); i++)
    {
        if(!m_listParamItems[i]->getIsDeleted())
        valuesList.append(m_listParamItems[i]->getValue());
    }
    return valuesList;
}

bool ABIParam::isValid()
{
    bool isValid = true;
    for(int i = 0; i < m_listParamItems.count(); i++)
    {
        if(!m_listParamItems[i]->getIsDeleted() && !m_listParamItems[i]->isValid())
            isValid = false;
    }
    return isValid;
}

void ABIParam::updateParamItemsPosition()
{
    for(int i = 0; i < m_paramItemsLayout->count(); i++)
    {
        m_listParamItems[i]->setPosition(i);
    }
}

void ABIParam::addNewParamItem(int position)
{
    if(m_listParamItems.count() == 1 && m_listParamItems[0]->getIsDeleted())
    {
        m_hSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        m_listParamItems[0]->setIsDeleted(false);
    }
    else
    {
        ABIParamItem *item = new ABIParamItem(m_param, this);
        m_listParamItems.insert(position, item);
        m_paramItemsLayout->insertWidget(position, item);

        if(m_paramItemsLayout->count() > 1)
        {
            m_vSpacer->changeSize(20, 40, QSizePolicy::Fixed, QSizePolicy::Expanding);
        }

        updateParamItemsPosition();

        connect(item, SIGNAL(on_addItemClicked(int)), this, SLOT(addNewParamItem(int)));
        connect(item, SIGNAL(on_removeItemClicked(int)), this, SLOT(removeParamItem(int)));
    }
}

void ABIParam::removeParamItem(int position)
{
    if(m_listParamItems.count() == 1)
    {
        m_listParamItems[0]->setIsDeleted(true);
        m_hSpacer->changeSize(40, 20, QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    else
    {
        QLayoutItem *item = m_paramItemsLayout->takeAt(position);
        QWidget * widget = item->widget();
        if(widget != NULL)
        {
            m_paramItemsLayout->removeWidget(widget);
            disconnect(widget, 0, 0, 0);
            widget->setParent(NULL);
            delete widget;
            m_listParamItems.removeAt(position);
        }

        if(m_paramItemsLayout->count() < 2)
        {
            m_vSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        }

        updateParamItemsPosition();
    }
}
