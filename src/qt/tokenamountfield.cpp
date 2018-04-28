#include "tokenamountfield.h"

#include "bitcoinunits.h"
#include "guiconstants.h"
#include "qvaluecombobox.h"

#include <QApplication>
#include <QAbstractSpinBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QHBoxLayout>

class TokenAmountSpinBox: public QAbstractSpinBox
{
    Q_OBJECT

public:
    explicit TokenAmountSpinBox(QWidget *parent):
        QAbstractSpinBox(parent),
        decimalUnits(0),
        totalSupply(0),
        singleStep(0),
        minAmount(0)
    {
        setAlignment(Qt::AlignRight);

        connect(lineEdit(), SIGNAL(textEdited(QString)), this, SIGNAL(valueChanged()));
    }

    QValidator::State validate(QString &text, int &pos) const
    {
        if(text.isEmpty())
            return QValidator::Intermediate;
        bool valid = false;
        parse(text, &valid);
        return valid ? QValidator::Intermediate : QValidator::Invalid;
    }

    void fixup(QString &input) const
    {
        bool valid = false;
        int256_t val = parse(input, &valid);
        val = getMax(val, minAmount);
        if(valid)
        {
            input = BitcoinUnits::formatToken(decimalUnits, val, false, BitcoinUnits::separatorAlways);
            lineEdit()->setText(input);
        }
    }

    int256_t value(bool *valid_out=0) const
    {
        return parse(text(), valid_out);
    }

    void setValue(const int256_t& value)
    {
        int256_t val = getMax(value, minAmount);
        lineEdit()->setText(BitcoinUnits::formatToken(decimalUnits, val, false, BitcoinUnits::separatorAlways));
        Q_EMIT valueChanged();
    }

    void stepBy(int steps)
    {
        bool valid = false;
        int256_t val = value(&valid);
        val = val + steps * singleStep;
        val = getMin(getMax(val, minAmount), totalSupply);
        setValue(val);
    }

    int256_t minimum() const
    {
        return minAmount;
    }

    void setMinimum(const int256_t& min)
    {
        minAmount = min;
        Q_EMIT valueChanged();
    }

    void setTotalSupply(const int256_t &value)
    {
        totalSupply = value;
    }

    void setDecimalUnits(int value)
    {
        decimalUnits = value;
        setSingleStep();
    }

private:
    int decimalUnits;
    int256_t totalSupply;
    int256_t singleStep;
    int256_t minAmount;

    int256_t parse(const QString &text, bool *valid_out=0) const
    {
        int256_t val = 0;
        bool valid = BitcoinUnits::parseToken(decimalUnits, text, &val);
        if(valid)
        {
            if(val < 0 || val > totalSupply)
                valid = false;
        }
        if(valid_out)
            *valid_out = valid;
        return valid ? val : 0;
    }

    void setSingleStep()
    {
        int256_t step = 1;
        for(int i = 1; i < decimalUnits; i++)
        {
            step *= 10;
        }
        singleStep = step;
    }

    int256_t getMax(int256_t a, int256_t b) const{
        return a > b ? a : b;
    }

    int256_t getMin(int256_t a, int256_t b) const{
        return a > b ? b : a;
    }

protected:
    bool event(QEvent *event)
    {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Comma)
            {
                QKeyEvent periodKeyEvent(event->type(), Qt::Key_Period, keyEvent->modifiers(), ".", keyEvent->isAutoRepeat(), keyEvent->count());
                return QAbstractSpinBox::event(&periodKeyEvent);
            }
        }
        return QAbstractSpinBox::event(event);
    }

    StepEnabled stepEnabled() const
    {
        if (isReadOnly()) 
            return StepNone;
        if (text().isEmpty()) 
            return StepUpEnabled;

        StepEnabled rv = 0;
        bool valid = false;
        int256_t val = value(&valid);
        if(valid)
        {
            if(val > minAmount)
                rv |= StepDownEnabled;
            if(val < totalSupply)
                rv |= StepUpEnabled;
        }
        return rv;
    }

Q_SIGNALS:
    void valueChanged();
};

#include "tokenamountfield.moc"

TokenAmountField::TokenAmountField(QWidget *parent) :
    QWidget(parent),
    amount(0)
{
    amount = new TokenAmountSpinBox(this);
    amount->setLocale(QLocale::c());
    amount->installEventFilter(this);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->addWidget(amount);

    layout->setContentsMargins(0,0,0,0);

    setLayout(layout);
    connect(amount, SIGNAL(valueChanged()), this, SIGNAL(valueChanged()));
}

void TokenAmountField::clear()
{
    amount->clear();
}

void TokenAmountField::setEnabled(bool fEnabled)
{
    amount->setEnabled(fEnabled);
}

bool TokenAmountField::validate()
{
    bool valid = false;
    value(&valid);
    setValid(valid);
    return valid;
}

void TokenAmountField::setValid(bool valid)
{
    if (valid)
        amount->setStyleSheet("");
    else
        amount->setStyleSheet(STYLE_INVALID);
}

bool TokenAmountField::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::FocusIn)
    {
        setValid(true);
    }
    return QWidget::eventFilter(object, event);
}

int256_t TokenAmountField::value(bool *valid_out) const
{
    return amount->value(valid_out);
}

void TokenAmountField::setValue(const int256_t& value)
{
    amount->setValue(value);
}

void TokenAmountField::setReadOnly(bool fReadOnly)
{
    amount->setReadOnly(fReadOnly);
}

int256_t TokenAmountField::minimum() const
{
    return amount->minimum();
}

void TokenAmountField::setMinimum(const int256_t& min)
{
    amount->setMinimum(min);
}

void TokenAmountField::setTotalSupply(const int256_t &value)
{
    amount->setTotalSupply(value);
}

void TokenAmountField::setDecimalUnits(int value)
{
    amount->setDecimalUnits(value);
}

QString TokenAmountField::text() const
{
    return QString::fromStdString(value().str());
}
