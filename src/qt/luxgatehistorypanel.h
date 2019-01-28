#ifndef LUXGATEHISTORYPANEL_H
#define LUXGATEHISTORYPANEL_H

#include <QFrame>

namespace Ui {
class LuxgateHistoryPanel;
}

class LuxgateHistoryPanel : public QFrame
{
    Q_OBJECT

public:
    explicit LuxgateHistoryPanel(QWidget *parent = nullptr);
    ~LuxgateHistoryPanel();

private:
    Ui::LuxgateHistoryPanel *ui;
};

#endif // LUXGATEHISTORYPANEL_H
