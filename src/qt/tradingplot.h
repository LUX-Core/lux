#ifndef WIDGET_H
#define WIDGET_H

#include <QFrame>
#include <QNetworkAccessManager>
#include <QPair>
#include <QList>
#include <QMap>
#include "qcustomplot.h"

namespace Ui {
class TradingPlot;
}


class TradingPlot : public QFrame
{
    Q_OBJECT

public:
    explicit TradingPlot(QWidget *parent = nullptr);
    ~TradingPlot() override;
    void updateChartData(bool bInit);
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum Rects {mainGraph, volumeBars, nRects};
    struct mouseTrackingRect
    {
        QCPCurve *vLine {nullptr};
        QCPCurve *hLine {nullptr};
        QCPItemText * text {nullptr};
        QCPAxisRect * rect {nullptr};
        void setVisible(bool bVis)
        {
            if(vLine)
                vLine->setVisible(bVis);
            if(hLine)
                hLine->setVisible(bVis);
            if(text)
                text->setVisible(bVis);
        }
    };

    Ui::TradingPlot *ui;
    QCPFinancial *ohlc{nullptr};
    QCPGraph * graph{nullptr};
    QCPGraph *baseLine{nullptr};
    bool bBaseLineDrag {false};
    mouseTrackingRect mouseTracks[nRects];
    QCPItemText * loadText {nullptr};
    QCPAxisRect *volumeAxisRect {nullptr};
    //use https://www.cryptopia.co.nz/Exchange/?market=XSN_BTC
    const QMap<QString, QStringList> enableIntervals;
    //use https://github.com/ccxt/ccxt/issues/2815
    const QList<QPair<QString, QString>> namRanges;
    const QList<QPair<QString, QString>> namIntervals;
    int rangeToInt(const QString &range);
    QString valueOfPairList(const QList<QPair<QString, QString>> & list,
                            const QString & key);
    QCPBars *volumeBar {nullptr};
    double dbMinTimeTradPlot {0};
    double dbMaxTimeTradPlot {0};

    void limitAxisRange(QCPAxis * axis, const QCPRange & newRange, const QCPRange & limitRange);
    void repaintMouseTracking(QPoint pos);
    void updateCurrentInfo(QPoint pos);
    void fillInFinancialData();

    QMap<double, double> baseVolumes;
    QCPFinancialDataContainer financialData;
    QSharedPointer<QCPBarsDataContainer>  barData {new QCPBarsDataContainer};
    QNetworkAccessManager * nam{nullptr};
private slots:
    void slotReplyFinished();
    void slotMouseMovePlot(QMouseEvent *event);
    void slotMousePressPlot(QMouseEvent * ev);
    void slotMouseReleasePlot(QMouseEvent * ev);
    void slotTradPlot_X_Range_changed(const QCPRange & newRange);
    void slotHorzScrollBarChanged(int value);
    void slotGraphTypeChanged(QString typeGraph);
};

#endif // WIDGET_H
