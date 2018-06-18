#include "qvalidatedtextedit.h"
#include "guiconstants.h"

#include <QValidator>

QValidatedTextEdit::QValidatedTextEdit(QWidget *parent) :
    QTextEdit(parent),
    valid(true),
    checkValidator(0),
    emptyIsValid(true),
    isValidManually(false)
{
    connect(this, SIGNAL(textChanged()), this, SLOT(markValid()));
    setStyleSheet("");
}

void QValidatedTextEdit::clear()
{
    setValid(true);
    QTextEdit::clear();
}

void QValidatedTextEdit::setCheckValidator(const QValidator *v)
{
    checkValidator = v;
}

bool QValidatedTextEdit::isValid()
{
    // use checkValidator in case the QValidatedTextEdit is disabled
    if (checkValidator)
    {
        QString address = toPlainText();
        int pos = 0;
        if (checkValidator->validate(address, pos) == QValidator::Acceptable)
            return true;
    }

    return valid;
}

void QValidatedTextEdit::setValid(bool _valid)
{
    if(_valid == this->valid)
    {
        return;
    }

    setStyleSheet(_valid ? "" : STYLE_INVALID);

    this->valid = _valid;
}

void QValidatedTextEdit::setEnabled(bool enabled)
{
    if (!enabled)
    {
        // A disabled QValidatedLineEdit should be marked valid
        setValid(true);
    }
    else
    {
        // Recheck validity when QValidatedLineEdit gets enabled
        checkValidity();
    }

    QTextEdit::setEnabled(enabled);
}

void QValidatedTextEdit::checkValidity()
{
    if (emptyIsValid && toPlainText().isEmpty())
    {
        setValid(true);
    }
    else if(isValidManually)
    {
        setValid(true);
    }
    else if (checkValidator)
        {
            QString address = toPlainText();
            int pos = 0;
            if (checkValidator->validate(address, pos) == QValidator::Acceptable)
                setValid(true);
            else
                setValid(false);
        }
    else
        setValid(false);
}

void QValidatedTextEdit::markValid()
{
    // As long as a user is typing ensure we display state as valid
    setValid(true);
}

void QValidatedTextEdit::focusInEvent(QFocusEvent *event)
{
    setValid(true);
    QTextEdit::focusInEvent(event);
}

void QValidatedTextEdit::focusOutEvent(QFocusEvent *event)
{
    checkValidity();
    QTextEdit::focusOutEvent(event);
}

bool QValidatedTextEdit::getIsValidManually() const
{
    return isValidManually;
}

void QValidatedTextEdit::setIsValidManually(bool value)
{
    isValidManually = value;
}

bool QValidatedTextEdit::getEmptyIsValid() const
{
    return emptyIsValid;
}

void QValidatedTextEdit::setEmptyIsValid(bool value)
{
    emptyIsValid = value;
}
