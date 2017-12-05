#ifndef SIDETOOLBAR_H
#define SIDETOOLBAR_H

#include <QWidget>

namespace Ui {
class sidetoolbar;
}

class sidetoolbar : public QWidget
{
    Q_OBJECT

public:
    explicit sidetoolbar(QWidget *parent = 0);
    ~sidetoolbar();

private slots:
    void on_sidetoolbar_destroyed();

private:
    Ui::sidetoolbar *ui;
};

#endif // SIDETOOLBAR_H
