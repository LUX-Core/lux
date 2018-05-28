#ifndef TOKENAMOUNTFIELD_H
#define TOKENAMOUNTFIELD_H

#include "amount.h"
#include <QWidget>

#include <boost/multiprecision/cpp_int.hpp>
using namespace boost::multiprecision;

class TokenAmountSpinBox;
class TokenAmountField : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(int256_t value READ value WRITE setValue NOTIFY valueChanged USER true)

public:
    explicit TokenAmountField(QWidget *parent = 0);
    int256_t value(bool *value=0) const;

    void setValue(const int256_t& value);
    void setSingleStep(const int256_t& step);
    void setReadOnly(bool fReadOnly);
    void setValid(bool valid);
    bool validate();
    void clear();
    void setEnabled(bool fEnabled);

    int256_t minimum() const;
    void setMinimum(const int256_t& min);

    void setTotalSupply(const int256_t &value);
    void setDecimalUnits(int value);

    QString text() const;

Q_SIGNALS:
    void valueChanged();

protected:

    bool eventFilter(QObject *object, QEvent *event);

public Q_SLOTS:

private:
    TokenAmountSpinBox *amount;
};

#endif // TOKENAMOUNTFIELD_H
