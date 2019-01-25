#ifndef RECEIVETOKENPAGE_H
#define RECEIVETOKENPAGE_H

#include <QWidget>

namespace Ui {
class ReceiveTokenPage;
}

class ReceiveTokenPage : public QWidget
{
    Q_OBJECT

public:
    explicit ReceiveTokenPage(QWidget *parent = 0);
    ~ReceiveTokenPage();


void setAddress(QString address);
void setSymbol(QString symbol);

private Q_SLOTS:

void on_copyAddressClicked();

private:
    Ui::ReceiveTokenPage *ui;
    QString m_address;

void createQRCode();
};

#endif // RECEIVETOKENPAGE_H
