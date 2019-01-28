#ifndef LUXGATECREATEORDERPANEL_H
#define LUXGATECREATEORDERPANEL_H

#include <QFrame>

namespace Ui {
class LuxgateCreateOrderPanel;
}

class LuxgateCreateOrderPanel : public QFrame
{
    Q_OBJECT

public:
    explicit LuxgateCreateOrderPanel(QWidget *parent = nullptr);
    ~LuxgateCreateOrderPanel();

private:
    Ui::LuxgateCreateOrderPanel *ui;
};

#endif // LUXGATECREATEORDERPANEL_H
