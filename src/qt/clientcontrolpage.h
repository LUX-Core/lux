#ifndef CLIENTCONTROLPAGE_H
#define CLIENTCONTROLPAGE_H

#include <QWidget>

namespace Ui {
    class ClientControlPage;
}
class ClientModel;
class WalletModel;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Overview ("home") page widget */
class ClientControlPage : public QWidget
{
    Q_OBJECT

public:
    explicit ClientControlPage(QWidget *parent = 0);
    ~ClientControlPage();
    void setModel(WalletModel *model);

public slots:

signals:

private:
    Ui::ClientControlPage *ui;
        WalletModel *model;

private slots:
        void on_chck4_upd8_clicked();
        void on_dwngrd_opt_clicked();
        void on_CS_submit_clicked();
        void on_BR_submit_clicked();
        void on_AU_apply_clicked();
        void on_minCLIE_clicked();
};

#endif // CLIENTCONTROLPAGE_H
