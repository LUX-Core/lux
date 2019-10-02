// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TRAFFICGRAPHWIDGET_H
#define BITCOIN_QT_TRAFFICGRAPHWIDGET_H

#include "trafficgraphdata.h"

#include <boost/function.hpp>

#include <QQueue>
#include <QWidget>

class ClientModel;

QT_BEGIN_NAMESPACE
class QPaintEvent;
class QTimer;
QT_END_NAMESPACE

class TrafficGraphWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TrafficGraphWidget(QWidget* parent = 0);
    ~TrafficGraphWidget();
    void setClientModel(ClientModel* model);
    int getGraphRangeMins() const;

protected:
    void paintEvent(QPaintEvent*);

public slots:
    void updateRates();
    void setGraphRangeMins(int value);
    void clear();

private:
    typedef boost::function<float(const TrafficSample&)> SampleChooser;
    void paintPath(QPainterPath &path, const TrafficGraphData::SampleQueue &queue, SampleChooser chooser);

    QTimer* timer;
    float fMax;
    int nMins;
    ClientModel* clientModel;
    TrafficGraphData trafficGraphData;
};

#endif // BITCOIN_QT_TRAFFICGRAPHWIDGET_H
