#ifndef LUXGATEBIDPANEL_H
#define LUXGATEBIDPANEL_H

#include <QFrame>
#include "ui_luxgatebidpanel.h"

namespace Ui {
class LuxgateBidPanel;
}

class LuxgateBidPanel : public QFrame
{
    Q_OBJECT

public:
    explicit LuxgateBidPanel(QWidget *parent = nullptr);
    ~LuxgateBidPanel();

private:
    Ui::LuxgateBidPanel *ui;
};

#endif // LUXGATEBIDPANEL_H
