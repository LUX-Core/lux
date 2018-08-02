#ifndef HEXADDRESSCONVERTER_H
#define HEXADDRESSCONVERTER_H

#include <QMainWindow>

namespace Ui {
class HexAddressConverter;
}

class HexAddressConverter : public QMainWindow
{
    Q_OBJECT

public:
    explicit HexAddressConverter(QWidget *parent = nullptr);
    ~HexAddressConverter();

private:
    Ui::HexAddressConverter *ui;

private Q_SLOTS:
    void addressChanged(const QString& address);
    void copyButtonClicked();
};

#endif // HEXADDRESSCONVERTER_H
