#ifndef CONTRACTPARAMFIELD_H
#define CONTRACTPARAMFIELD_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include "qvalidatedlineedit.h"
#include "contractabi.h"
#include "abiparamitem.h"

class PlatformStyle;
class ParameterABI;
/**
 * @brief The ABIParam class ABI parameter widget
 */
class ABIParam : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief ABIParam Constructor
     * @param ID Id if the parameter
     * @param name Name of the parameter
     * @param parent Parent windows for the GUI control
     */
    explicit ABIParam(const PlatformStyle *platformStyle, int ID, const ParameterABI &param, QWidget *parent = 0);

    /**
     * @brief getValue Get the value of the parameter
     * @return String representation of the value
     */
    QStringList getValue();

    bool isValid();
    void updateParamItemsPosition();

Q_SIGNALS:

public Q_SLOTS:
    void addNewParamItem(int position);
    void removeParamItem(int position);

private:
    int m_ParamID;
    QLabel *m_paramName;
    QHBoxLayout *m_mainLayout;
    QVBoxLayout *m_paramItemsLayout;
    ParameterABI m_param;
    QList<ABIParamItem*> m_listParamItems;
    const PlatformStyle *m_platformStyle;
    QSpacerItem *m_vSpacer;
    QSpacerItem *m_hSpacer;
};

#endif // CONTRACTPARAMFIELD_H
