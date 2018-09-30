// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef I2POPTIONS_H
#define I2POPTIONS_H

#include <QWidget>

class MonitoredDataMapper;

namespace Ui {
    class I2POptions;
}

class ClientModel;

class I2POptions : public QWidget
{
    Q_OBJECT

public:
    explicit I2POptions(QWidget *parent = 0);
    ~I2POptions();

    void setMapper(MonitoredDataMapper& mapper);
    void setModel(ClientModel* model);

private:
    Ui::I2POptions *ui;
    ClientModel* clientModel;

private slots:
            void ShowCurrentI2PAddress();
    void GenerateNewI2PAddress();

    signals:
            void settingsChanged();
};

#endif // I2POPTIONS_H
