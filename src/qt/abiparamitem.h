#ifndef ABIPARAMITEM_H
#define ABIPARAMITEM_H

#include <QWidget>
#include <QToolButton>
#include "qvalidatedlineedit.h"

class PlatformStyle;
class ParameterABI;

class ABIParamItem : public QWidget
{
    Q_OBJECT
public:

    explicit ABIParamItem(const PlatformStyle *platformStyle, const ParameterABI &param, QWidget *parent = 0);

    QString getValue();
    void setFixed(bool isFixed);

    int getPosition() const;
    void setPosition(int position);

    bool getIsDeleted() const;
    void setIsDeleted(bool isLast);

    bool isValid();

Q_SIGNALS:
    void on_addItemClicked(int position);
    void on_removeItemClicked(int position);

public Q_SLOTS:
    void on_addItemClicked();
    void on_removeItemClicked();

private:
    QToolButton *m_buttonAdd;
    QToolButton *m_buttonRemove;
    QValidatedLineEdit *m_itemValue;
    int m_position;
    bool m_isDeleted;
};

#endif // ABIPARAMITEM_H
