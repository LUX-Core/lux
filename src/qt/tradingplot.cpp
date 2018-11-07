#include "tradingplot.h"
#include "ui_tradingplot.h"
#include <QUrl>
#include <QNetworkReply>
#include <QEvent>
#include <QDebug>
#include <QCursor>

#define defInitProperty "Init"
#define defDataRangeProperty "DataRange"
#define defTimeFormat "dd.MM.yy(hh:mm)"

TradingPlot::TradingPlot(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TradingPlot),
    enableIntervals {   {"1d",{"15m", "30m", "1hr", "2hr"}},
                        {"2d",{"15m", "30m", "1hr", "2hr"}},
                        {"1w",{"1hr", "2hr", "4hr", "12hr"}},
                        {"2w",{"2hr", "4hr", "12hr"}},
                        {"1m",{"4hr", "12hr", "1d"}},
                        {"3m",{"4hr", "12hr", "1d", "1w"}},
                        {"6m",{"12hr", "1d", "1w"}},
                        {"All",{"1d", "1w"}}},
    namRanges { {"1d", "0"}, {"2d", "1"}, {"1w", "2"},
                {"2w", "3"}, {"1m", "4"}, {"3m", "5"},
                {"6m", "6"}, {"All", "7"}},
    namIntervals{   {"15m", "15"}, {"30m", "30"}, {"1hr", "60"},
                    {"2hr", "120"}, {"4hr", "240"}, {"12hr", "720"},
                    {"1d", "1440"}, {"1w", "10080"}}
{
    ui->setupUi(this);
    ui->priceChart->installEventFilter(this);
    ui->horPriceChartScrollBar->setRange(0, 100);

    ui->priceChart->setInteractions(QCP::iRangeDrag|QCP::iRangeZoom);
    ui->priceChart->setMouseTracking(true);
    ui->priceChart->yAxis->setLabel("LUX Price in BTC");

    ohlc = new QCPFinancial(ui->priceChart->xAxis, ui->priceChart->yAxis);
    ohlc->setName("OHLC");
    ohlc->setTwoColored(true);

    connect(ui->priceChart, &QCustomPlot::mouseMove,
            this, &TradingPlot::slotMouseMovePlot);
    connect(ui->priceChart, &QCustomPlot::mousePress,
            this, &TradingPlot::slotMousePressPlot);
    connect(ui->priceChart, &QCustomPlot::mouseRelease,
            this, &TradingPlot::slotMouseReleasePlot);
    connect(ui->priceChart->xAxis, SIGNAL(rangeChanged(QCPRange)),
            this, SLOT(slotTradPlot_X_Range_changed(QCPRange)));
    connect(ui->horPriceChartScrollBar, SIGNAL(valueChanged(int)),
            this, SLOT(slotHorzScrollBarChanged(int)));

    loadText = new QCPItemText(ui->priceChart);
    loadText->setClipToAxisRect(false);
    loadText->position->setType(QCPItemPosition::ptPlotCoords);
    loadText->setFont(QFont(font().family(), 16, QFont::Normal, true));
    loadText->setPen(Qt::NoPen);
    loadText->setLayer("overlay");
    loadText->position->setCoords(ui->priceChart->xAxis->range().center(),
                                  ui->priceChart->yAxis->range().center());
    loadText->setText("Loading...");

    // create bottom axis rect for volume bar chart:
    volumeAxisRect = new QCPAxisRect(ui->priceChart);
    volumeAxisRect->setRangeDrag(Qt::Horizontal);
    volumeAxisRect->setRangeZoom(Qt::Horizontal);
    ui->priceChart->axisRect(0)->setRangeDrag(Qt::Horizontal);
    ui->priceChart->axisRect(0)->setRangeZoom(Qt::Horizontal);
    ui->priceChart->plotLayout()->addElement(1, 0, volumeAxisRect);
    volumeAxisRect->setMaximumSize(QSize(QWIDGETSIZE_MAX, 100));
    volumeAxisRect->axis(QCPAxis::atBottom)->setLayer("axes");
    volumeAxisRect->axis(QCPAxis::atBottom)->grid()->setLayer("grid");
    volumeAxisRect->axis(QCPAxis::atBottom)->setLabel("Time (GMT)");
    // bring bottom and main axis rect closer together:
    ui->priceChart->plotLayout()->setRowSpacing(0);
    volumeAxisRect->setAutoMargins(QCP::msLeft|QCP::msRight|QCP::msBottom);
    volumeAxisRect->setMargins(QMargins(0, 0, 0, 0));
    ui->priceChart->setAutoAddPlottableToLegend(false);
    volumeBar = new QCPBars(volumeAxisRect->axis(QCPAxis::atBottom), volumeAxisRect->axis(QCPAxis::atLeft));
    volumeBar->setPen(Qt::NoPen);
    volumeBar->setBrush(QColor(Qt::blue));

    // interconnect x axis ranges of main and bottom axis rects:
    connect(ui->priceChart->xAxis, SIGNAL(rangeChanged(QCPRange)),
            volumeAxisRect->axis(QCPAxis::atBottom), SLOT(setRange(QCPRange)));
    connect(volumeAxisRect->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange)),
            ui->priceChart->xAxis, SLOT(setRange(QCPRange)));

    QSharedPointer<QCPAxisTickerDateTime> dateTimeTicker(new QCPAxisTickerDateTime);
    dateTimeTicker->setDateTimeSpec(Qt::UTC);
    dateTimeTicker->setDateTimeFormat(defTimeFormat);
    ui->priceChart->xAxis->setBasePen(Qt::NoPen);
    ui->priceChart->xAxis->setTickLabels(false);
    ui->priceChart->xAxis->setTicks(false);
    ui->priceChart->xAxis->setTicker(dateTimeTicker);
    volumeAxisRect->axis(QCPAxis::atBottom)->setTicker(dateTimeTicker);
    volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabelRotation(15);
    ui->priceChart->rescaleAxes();

    // make axis rects' left side line up:
    QCPMarginGroup *group = new QCPMarginGroup(ui->priceChart);
    ui->priceChart->axisRect()->setMarginGroup(QCP::msLeft|QCP::msRight, group);
    volumeAxisRect->setMarginGroup(QCP::msLeft|QCP::msRight, group);

    ui->priceChart->plotLayout()->setRowSpacing(0);

    //fill graph
    graph = ui->priceChart->addGraph(ui->priceChart->xAxis,
                                     ui->priceChart->yAxis);
    graph->setPen(QPen(Qt::darkCyan));

    graph->setVisible(false);

    //fill baseline
    baseLine = new QCPGraph(ui->priceChart->xAxis,
                            ui->priceChart->yAxis);
    baseLine->setPen(QPen(QBrush(Qt::black), 2, Qt::DotLine));

    //fill mouseTracks
    mouseTracks[mainGraph].rect = ui->priceChart->axisRect();
    mouseTracks[volumeBars].rect = volumeAxisRect;
    for(int i=0; i<nRects; i++)
    {
        //vLine
        mouseTracks[i].vLine = new QCPCurve(mouseTracks[i].rect->axis(QCPAxis::atBottom),
                                            mouseTracks[i].rect->axis(QCPAxis::atLeft));
        mouseTracks[i].vLine->setPen(QPen(QBrush(Qt::gray), 1, Qt::DashDotDotLine));

        //hLine
        mouseTracks[i].hLine = new QCPCurve(mouseTracks[i].rect->axis(QCPAxis::atBottom),
                                            mouseTracks[i].rect->axis(QCPAxis::atLeft));
        mouseTracks[i].hLine->setPen(QPen(QBrush(Qt::gray), 1, Qt::DashDotDotLine));

        //text
        mouseTracks[i].text = new QCPItemText(ui->priceChart);
        mouseTracks[i].text->setPositionAlignment(Qt::AlignBottom|Qt::AlignLeft);
        mouseTracks[i].text->position->setType(QCPItemPosition::ptPlotCoords);
        mouseTracks[i].text->setFont(QFont(font().family(), 8, QFont::Light, true));
        mouseTracks[i].text->setPen(QPen(Qt::gray));
        mouseTracks[i].text->setLayer("overlay");
        mouseTracks[i].text->setClipAxisRect(mouseTracks[i].rect);
        mouseTracks[i].text->position->setAxes(mouseTracks[i].rect->axis(QCPAxis::atBottom),
                                               mouseTracks[i].rect->axis(QCPAxis::atLeft));
        mouseTracks[i].setVisible(false);
    }

    //fill comboBoxes
    foreach(auto pair, namRanges)
        ui->comboBoxDataRange->addItem(pair.first);
    foreach(auto pair, namIntervals)
        ui->comboBoxDataInterval->addItem(pair.first);

    connect(ui->comboBoxDataRange, QOverload<const QString &>::of(&QComboBox::activated),
          [=](const QString &text){ Q_UNUSED(text); updateChartData(true); });
    connect(ui->comboBoxDataInterval, QOverload<const QString &>::of(&QComboBox::activated),
          [=](const QString &text){ Q_UNUSED(text); updateChartData(true); });

    ui->comboBoxGraphType->addItems(QStringList()<< tr("Bars")<< tr("Candles")
                                    << tr("Heikin Ashi") << tr("Line") << tr("Area")
                                    << tr("Baseline"));
    connect(ui->comboBoxGraphType, &QComboBox::currentTextChanged,
          this, &TradingPlot::slotGraphTypeChanged);
    ui->comboBoxDataRange->setCurrentText("Bars");
    slotGraphTypeChanged("Bars");
    updateChartData(true);
}

void TradingPlot::slotGraphTypeChanged(QString typeGraph)
{
    ohlc->setVisible(false);
    graph->setVisible(false);
    baseLine->setVisible(false);
    graph->setChannelFillGraph(nullptr);
    if(tr("Bars")==typeGraph)
    {
        ohlc->setChartStyle(QCPFinancial::csOhlc);
        ohlc->setVisible(true);
    }
    if(tr("Candles")==typeGraph || tr("Heikin Ashi")==typeGraph)
    {
        ohlc->setChartStyle(QCPFinancial::csCandlestick);
        ohlc->setVisible(true);
    }
    if(tr("Line")==typeGraph)
    {
        graph->setBrush(Qt::NoBrush);
        graph->setVisible(true);
    }
    if(tr("Area")==typeGraph)
    {
        graph->setBrush(QBrush(Qt::cyan));
        graph->setVisible(true);
    }
    if(tr("Baseline")==typeGraph)
    {
        graph->setBrush(QBrush(Qt::cyan));
        graph->setVisible(true);
        graph->setChannelFillGraph(baseLine);
        baseLine->setVisible(true);
        QVector<double> xH(2), yH(2);
            xH[0] = dbMinTimeTradPlot;
            yH[0] = ui->priceChart->axisRect()->axis(QCPAxis::atLeft)->range().center();
            xH[1] = dbMaxTimeTradPlot;
            yH[1] = ui->priceChart->axisRect()->axis(QCPAxis::atLeft)->range().center();

        baseLine->setData(xH, yH);      
    }
    fillInFinancialData();
    ui->priceChart->replot();
}

bool TradingPlot::eventFilter(QObject *watched, QEvent *ev)
{
    if(watched == ui->priceChart)
    {
        if(ev->type()==QEvent::Leave)
        {
            for(int i=0; i<nRects; i++)
                mouseTracks[i].setVisible(false);
            ui->priceChart->setCursor(Qt::ArrowCursor);
            ui->priceChart->replot();
        }
    }
    return QWidget::eventFilter(watched, ev);
}

int TradingPlot::rangeToInt(const QString & range)
{
    int res{-1};
    if(range.contains("d"))
        res = QString(range).remove("d").toInt()*24*60*60;
    if(range.contains("w"))
        res = QString(range).remove("w").toInt()*7*24*60*60;
    if(range.contains("m"))
        res = QString(range).remove("m").toInt()*30*24*60*60;
    return res;
}

QString TradingPlot::valueOfPairList(
        const QList<QPair<QString, QString>> & list,
                        const QString &  key)
{
    QString res;
    foreach(auto pair, list)
    {
        if(key == pair.first)
        {
            res = pair.second;
            break;
        }
    }
    return res;
}

//TODO: Is tradePairId==const? If no 1)https://www.cryptopia.co.nz/api/GetMarket/LUX_BTC
//2) this request
void TradingPlot::updateChartData(bool bInit)
{
    //disable items
    QStandardItemModel *model =
          qobject_cast<QStandardItemModel *>(ui->comboBoxDataInterval->model());
    QString dataRange = ui->comboBoxDataRange->currentText();
    QString dataInterval = ui->comboBoxDataInterval->currentText();
    if(!enableIntervals[dataRange].contains(dataInterval))
    {
        dataInterval = enableIntervals[dataRange][0];
        ui->comboBoxDataInterval->setCurrentText(enableIntervals[dataRange][0]);
    }
    for(int i=0; i<ui->comboBoxDataInterval->count(); i++)
    {
        QStandardItem *item = model->item(i);
        if(enableIntervals[dataRange]
                .contains(ui->comboBoxDataInterval->itemText(i)))
        {
            item->setFlags(item->flags() | Qt::ItemIsEnabled);
        }
        else
        {
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        }
    }
    nam = new QNetworkAccessManager(this);

    QString url = "https://www.cryptopia.co.nz/Exchange/"
                  "GetTradePairChart?tradePairId=5643&dataRange="
                  + valueOfPairList(namRanges, dataRange)
                  + "&dataGroup=" + valueOfPairList(namIntervals, dataInterval);
    QNetworkRequest req = QNetworkRequest(QUrl(url));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto reply = nam->get(req);
    reply->setProperty(defInitProperty, bInit);
    reply->setProperty(defDataRangeProperty, rangeToInt(dataRange));
    connect( reply, &QNetworkReply::finished,
             this, &TradingPlot::slotReplyFinished);
    loadText->position->setCoords(ui->priceChart->xAxis->range().center(),
                                  ui->priceChart->yAxis->range().center());
    loadText->setVisible(true);
    ui->priceChart->replot();
}

void TradingPlot::slotReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    bool bInitTradingPlot = reply->property(defInitProperty).toBool();
    int iInitRange = reply->property(defDataRangeProperty).toInt();
    QString Response;
    if (reply->error() == QNetworkReply::NoError) {
        //success
        Response = reply->readAll();

    }
    else
        return;

    QJsonArray allCandles = QJsonDocument::fromJson(Response.toUtf8()).
            object()["Candle"].toArray();

    QJsonArray allVolumes = QJsonDocument::fromJson(Response.toUtf8()).
            object()["Volume"].toArray();

    if(allCandles.size() < 2)
        return;

    financialData.clear();

    for(int i=0; i<allCandles.size(); i++)
        financialData.add(QCPFinancialData(
                allCandles[i].toArray()[0].toDouble()/1000,
                allCandles[i].toArray()[1].toDouble(),
                allCandles[i].toArray()[2].toDouble(),
                allCandles[i].toArray()[3].toDouble(),
                allCandles[i].toArray()[4].toDouble()));
    fillInFinancialData();

    barData->clear();
    baseVolumes.clear();
    for (int i=0; i<allVolumes.size(); i++)
    {
      double timestamp = allVolumes[i].toObject()["x"].toDouble()/1000;
      double volume = allVolumes[i].toObject()["y"].toDouble();
      double base = allVolumes[i].toObject()["basev"].toDouble();
      baseVolumes[timestamp] = base;
      barData->add(QCPBarsData(timestamp,volume));
    }
    volumeBar->setData(barData);
    //set ranges only first time
    if(bInitTradingPlot)
    {
        loadText->setVisible(false);
        QString dataInterval =ui->comboBoxDataInterval->currentText();
        double binSize = valueOfPairList(namIntervals, dataInterval)
                .toInt()*60;
        ohlc->setWidth(binSize*0.2);
        volumeBar->setWidth(binSize*0.2);
        //calc min and max values
        dbMinTimeTradPlot   = (*financialData.begin()).key;
        dbMaxTimeTradPlot   = (*financialData.begin()).key;
        for (auto it = financialData.begin();
             it != financialData.end(); it++)
        {
            if((*it).key > dbMaxTimeTradPlot)
                dbMaxTimeTradPlot = (*it).key;
            if((*it).key < dbMinTimeTradPlot)
                dbMinTimeTradPlot = (*it).key;
        }

        if(iInitRange != -1)
            ui->priceChart->xAxis->setRange(dbMaxTimeTradPlot - iInitRange,
                                            dbMaxTimeTradPlot);
        else
            ui->priceChart->xAxis->setRange(dbMinTimeTradPlot,
                                            dbMaxTimeTradPlot);
    }
    ui->priceChart->replot();
}

void TradingPlot::fillInFinancialData()
{
    QString typeGraph = ui->comboBoxGraphType->currentText();
    if(tr("Bars")==typeGraph
            ||
       tr("Candles")==typeGraph)
    {
        ohlc->data()->set(financialData);
    }
    if(tr("Heikin Ashi")==typeGraph)
    {
        QCPFinancialDataContainer haFinDatas;

        haFinDatas.add(*financialData.begin());
        //fill in haFinData
        {
            QCPFinancialData prevData = *(financialData.begin());
            for(auto it = financialData.begin()+1;
                it!=financialData.end();
                it++)
            {
                QCPFinancialData data;
                data.open = (prevData.open + prevData.close)/2;
                data.close = ((*it).open + (*it).close + (*it).high + (*it).low)/4;
                data.low = std::min({data.open, data.close, (*it).low});
                data.high = std::max({data.open, data.close, (*it).high});
                data.key = (*it).key;
                haFinDatas.add(data);
                prevData = data;
            }
        }
        ohlc->data()->set(haFinDatas);
    }
    if(tr("Line")==typeGraph ||
       tr("Area")==typeGraph ||
       tr("Baseline")==typeGraph)
    {
        QVector<double> x, y;
        for(auto it = financialData.begin();
            it!=financialData.end();
            it++)
        {
            x.push_back(it->key);
            y.push_back(it->open);
        }
        graph->setData(x,y);
    }
}

void TradingPlot::updateCurrentInfo(QPoint pos)
{
    auto rect = ui->priceChart->axisRectAt(pos);
    if(rect == nullptr)
        return;
    double coordX = rect->axis(QCPAxis::atBottom)->pixelToCoord(pos.x());
    auto financialData = ohlc->data();
    auto volumeData = volumeBar->data();
    if(financialData.isNull()
            ||volumeData.isNull())
        return;

    auto currentFin = (*financialData->findBegin(coordX, true));
    ui->labelClose->setText("<span style='font-weight:bold; font-size:12px; color:blue'>"
                            + QString::number(currentFin.close,'f',8));
    ui->labelOpen->setText("<span style='font-weight:bold; font-size:12px; color:blue'>"
                            + QString::number(currentFin.open,'f',8));
    ui->labelHigh->setText("<span style='font-weight:bold; font-size:12px; color:blue'>"
                            + QString::number(currentFin.high,'f',8));
    ui->labelLow->setText("<span style='font-weight:bold; font-size:12px; color:blue'>"
                            + QString::number(currentFin.low,'f',8));

    auto currentVol = (*volumeData->findBegin(coordX, true));
    ui->labelVol->setText("<span style='font-weight:bold; font-size:12px; color:blue'>"
                           + QString::number(currentVol.value,'f',8));

    QString strDate = QDateTime::fromSecsSinceEpoch(
                static_cast<qint64>(currentFin.key)).toString(defTimeFormat);
    ui->labelDate->setText("<span style='font-weight:bold; font-size:12px; color:blue'>"
                           + strDate);
    ui->labelBase->setText("<span style='font-weight:bold; font-size:12px; color:blue'>"
                           + QString::number(baseVolumes[currentVol.key],'f',8));

}

void TradingPlot::slotHorzScrollBarChanged(int value)
{
    double newLower = (value/100.0) * (dbMaxTimeTradPlot - dbMinTimeTradPlot
                                        - ui->priceChart->xAxis->range().size())
                    +dbMinTimeTradPlot;
    // if user is dragging plot, we don't want to replot twice
    if (qAbs(ui->priceChart->xAxis->range().lower-newLower) > 0.1)
    {
        ui->priceChart->xAxis->setRange(newLower, ui->priceChart->xAxis->range().size()+newLower);
        ui->priceChart->replot();
    }

}

void TradingPlot::limitAxisRange(QCPAxis * axis, const QCPRange & newRange, const QCPRange & limitRange)
{
    auto lowerBound = limitRange.lower;
    auto upperBound = limitRange.upper;

    // code assumes upperBound > lowerBound
    QCPRange fixedRange(newRange);
    if (fixedRange.lower < lowerBound)
    {
        fixedRange.lower = lowerBound;
        fixedRange.upper = lowerBound + newRange.size();
        if (fixedRange.upper > upperBound || qFuzzyCompare(newRange.size(), upperBound-lowerBound))
            fixedRange.upper = upperBound;
        axis->setRange(fixedRange); // adapt this line to use your plot/axis
    } else if (fixedRange.upper > upperBound)
    {
        fixedRange.upper = upperBound;
        fixedRange.lower = upperBound - newRange.size();
        if (fixedRange.lower < lowerBound || qFuzzyCompare(newRange.size(), upperBound-lowerBound))
            fixedRange.lower = lowerBound;
        axis->setRange(fixedRange); // adapt this line to use your plot/axis
    }
}

void TradingPlot::slotTradPlot_X_Range_changed(const QCPRange & newRange)
{
    limitAxisRange(ui->priceChart->xAxis,newRange,
                   QCPRange(dbMinTimeTradPlot,dbMaxTimeTradPlot));
    ui->horPriceChartScrollBar->setValue(qRound(
    (newRange.lower - dbMinTimeTradPlot)*100.0
    / (dbMaxTimeTradPlot - dbMinTimeTradPlot - newRange.size())));
    ui->horPriceChartScrollBar->setPageStep(qRound(
    (newRange.size()/(dbMaxTimeTradPlot - dbMinTimeTradPlot))*2000.0));

    //new price range
    {
        auto financialData = ohlc->data();
        double max_price = (*financialData->findBegin(newRange.lower, true)).high;
        double min_price = (*financialData->findBegin(newRange.lower, true)).low;
        for (auto it = financialData->findBegin(newRange.lower, true);
             it != financialData->findBegin(newRange.upper); it++)
        {
            if((*it).high > max_price)
                max_price = (*it).high;
            if((*it).low < min_price)
                min_price = (*it).low;
        }
        auto diff_price = max_price - min_price;
        ui->priceChart->yAxis->setRange(QCPRange(min_price - 0.2*diff_price,
                                                 max_price + 0.2*diff_price));
    }
    //new volume range
    {
        auto volumeData = volumeBar->data();
        double max_vol = (*volumeData->findBegin(newRange.lower, true)).value;
        double min_vol = (*volumeData->findBegin(newRange.lower, true)).value;
        for (auto it = volumeData->findBegin(newRange.lower, true);
             it != volumeData->findBegin(newRange.upper); it++)
        {
            if((*it).value > max_vol)
                max_vol = (*it).value;
            if((*it).value < min_vol)
                min_vol = (*it).value;
        }
        auto diff_vol = max_vol - min_vol;
        volumeAxisRect->axis(QCPAxis::atLeft)
         ->setRange(QCPRange(min_vol - 0.2*diff_vol,
                             max_vol + 0.2*diff_vol));
    }
}

void TradingPlot::slotMouseMovePlot(QMouseEvent *event)
{
    if(ui->priceChart->axisRect()==ui->priceChart->axisRectAt(event->pos())
            && baseLine->visible()
            && baseLine->selectTest(event->pos(), true)<ui->priceChart->selectionTolerance()
            && baseLine->selectTest(event->pos(), true)>0)
    {
        if(!bBaseLineDrag)
            ui->priceChart->setCursor(Qt::OpenHandCursor);
        else
            ui->priceChart->setCursor(Qt::ClosedHandCursor);
    }
    else
        ui->priceChart->setCursor(Qt::ArrowCursor);
    if(bBaseLineDrag)
    {
        QVector<double> xH(2), yH(2);
            xH[0] = dbMinTimeTradPlot;
            yH[0] = ui->priceChart->yAxis->pixelToCoord(event->pos().y());
            xH[1] = dbMaxTimeTradPlot;
            yH[1] = ui->priceChart->yAxis->pixelToCoord(event->pos().y());

        baseLine->setData(xH, yH);
    }
    repaintMouseTracking(event->pos());
    updateCurrentInfo(event->pos());
}

void TradingPlot::slotMousePressPlot(QMouseEvent * ev)
{
    if(ui->priceChart->axisRect()==ui->priceChart->axisRectAt(ev->pos())
            && baseLine->visible())
    {
        if(baseLine->selectTest(ev->pos(), true)<ui->priceChart->selectionTolerance()
           && baseLine->selectTest(ev->pos(), true)>0)
        {
            bBaseLineDrag = true;
            ui->priceChart->setCursor(Qt::ClosedHandCursor);
        }
    }
}

void TradingPlot::slotMouseReleasePlot(QMouseEvent * ev)
{
    Q_UNUSED(ev);
    if(bBaseLineDrag)
    {
        bBaseLineDrag = false;
        ui->priceChart->setCursor(Qt::OpenHandCursor);
    }
}

void TradingPlot::repaintMouseTracking(QPoint pos)
{
    for(int i=0; i<nRects; i++)
   {
       double coordX = mouseTracks[i].rect->axis(QCPAxis::atBottom)->pixelToCoord(pos.x());
       double coordY = mouseTracks[i].rect->axis(QCPAxis::atLeft)->pixelToCoord(pos.y());

       if(mouseTracks[i].rect
               ==ui->priceChart->axisRectAt(pos))
       {
            mouseTracks[i].setVisible(true);
            QVector<double> xH(2), yH(2);
                xH[0] = mouseTracks[i].rect->axis(QCPAxis::atBottom)->range().lower;
                yH[0] = coordY;
                xH[1] = mouseTracks[i].rect->axis(QCPAxis::atBottom)->range().upper;
                yH[1] = coordY;

            mouseTracks[i].hLine->setData(xH, yH);

            mouseTracks[i].text->position
                    ->setCoords(mouseTracks[i].rect->axis(QCPAxis::atBottom)->range().lower
                                + mouseTracks[i].rect->axis(QCPAxis::atBottom)->range().size()/200,
                                 coordY);
            int nPrec = 6;
            if(volumeBars == i)
                nPrec = 0;
            mouseTracks[i].text->setText(QString::number(coordY, 'f', nPrec));
       }
       else
            mouseTracks[i].setVisible(false);

       //vertical line allways visible
       QVector<double> xV(2), yV(2);
           xV[0] = coordX;
           yV[0] = mouseTracks[i].rect->axis(QCPAxis::atLeft)->range().lower;
           xV[1] = coordX;
           yV[1] = mouseTracks[i].rect->axis(QCPAxis::atLeft)->range().upper;
       mouseTracks[i].vLine->setVisible(true);
       mouseTracks[i].vLine->setData(xV,yV);

   }
    ui->priceChart->replot();
}

TradingPlot::~TradingPlot()
{
    delete ui;
}
