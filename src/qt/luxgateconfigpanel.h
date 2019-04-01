#ifndef LUXGATECONFIGPANEL_H
#define LUXGATECONFIGPANEL_H

#include <QFrame>

namespace Ui {
class LuxgateConfigPanel;
}

class LuxgateConfigPanel : public QFrame
{
    Q_OBJECT

public:
    explicit LuxgateConfigPanel(QWidget *parent = nullptr);
    ~LuxgateConfigPanel();

private:
    Ui::LuxgateConfigPanel *ui;
    void fillInTableWithConfig(QString strConfig = ""); //strConfig - path to import config, if strConfig == "" use standard config file

private slots:
    void slotClickAddConfiguration();
    void slotClickRemoveConfiguration();
    void slotClickResetConfiguration();
    void slotClickImportConfiguration();
    void slotClickExportConfiguration();
    void slotClickUpdateConfig();
    void slotConfigDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);
};

#endif // LUXGATECONFIGPANEL_H
