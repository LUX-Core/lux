#include "qvaluecombobox.h"

#include <QStyledItemDelegate>

QValueComboBox::QValueComboBox(QWidget *parent) :
        QComboBox(parent), role(Qt::UserRole)
{
    setItemDelegate(new QStyledItemDelegate());

    connect(this, SIGNAL(currentIndexChanged(int)), this, SLOT(handleSelectionChanged(int)));
}

QVariant QValueComboBox::value() const
{
    return itemData(currentIndex(), role);
}

void QValueComboBox::setValue(const QVariant &value)
{
    setCurrentIndex(findData(value, role));
}

void QValueComboBox::setRole(int role)
{
    this->role = role;
}

void QValueComboBox::handleSelectionChanged(int idx)
{
    emit valueChanged();
}
