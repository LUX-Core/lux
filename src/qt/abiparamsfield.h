#ifndef CONTRACTFUNCTIONFIELD_H
#define CONTRACTFUNCTIONFIELD_H

#include "QWidget"
#include "QList"
#include "QVBoxLayout"

#include "contractabi.h"

class ABIParam;

/**
 * @brief The ABIParamsField class List of parameters for contract function
 */
class ABIParamsField : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief ABIParamsField Constructor
     * @param parent Parent windows of the GUI control
     */
    explicit ABIParamsField(QWidget *parent = 0);

    /**
     * @brief updateParamsField Populate the GUI control with function parameters
     * @param function Contract function interface
     */
    void updateParamsField(const FunctionABI &function);

    /**
     * @brief getParamValue Get the value of a specific parameter
     * @param paramID Number of the parameter into the input list (0, 1, ...)
     * @return Value of the parameter
     */
    QStringList getParamValue(int paramID);

    /**
     * @brief getParamsValues Get the values of the whole list of input parameters
     * @return Values of the parameters
     */
    QList<QStringList> getParamsValues();

    bool isValid();

Q_SIGNALS:

public Q_SLOTS:

private:
    QVBoxLayout *m_mainLayout;
    QList<ABIParam*> m_listParams;
};

#endif // CONTRACTFUNCTIONFIELD_H
