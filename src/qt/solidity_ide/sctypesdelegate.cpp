#include "sctypesdelegate.h"
#include <QPainter>
#include <QPaintDevice>
#include <QFile>
#include <QApplication>
#include "sctypewidget.h"


//Shadow parameters
//Blur radiuses
#define RADIUS 10.0
#define RADIUS_SELECTED 15.0
//width of shadow
#define DISTANCE 5.0


//height of widget (selected)
#define defHeightWgtSelected 105
//height of widget (unselected)
#define defHeightWgtUnSelected 100
//vertical padding of widget (top)
#define defPaddingTop 5
//height of item (without spacing from qlistview)
#define defHeightItem defHeightWgtSelected + DISTANCE + defPaddingTop + 5
//padding of widget in qlistview
#define defPaddingHor 25
#define defPaddingHorSelected 22

QT_BEGIN_NAMESPACE
  extern Q_WIDGETS_EXPORT void qt_blurImage( QPainter *p, QImage &blurImage, qreal radius, bool quality, bool alphaOnly, int transposed = 0 );
QT_END_NAMESPACE


ScTypesDelegate::ScTypesDelegate(QObject *parent):
    QStyledItemDelegate(parent)
{
}

void ScTypesDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
           const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    this->initStyleOption(&opt, index);

    painter->save();
    painter->eraseRect(option.rect);
    double dbRadius = RADIUS;
    int paddingHor =defPaddingHor;
    int heightWgt = defHeightWgtUnSelected;
    if(option.state & QStyle::State_MouseOver)
    {
        dbRadius = RADIUS_SELECTED;
        paddingHor = defPaddingHorSelected;
        heightWgt = defHeightWgtSelected;
    }

    QSize sz = QSize(option.rect.width()-paddingHor*2, heightWgt);
    QPoint offset(option.rect.x()+paddingHor, option.rect.y()+ defPaddingTop);

    // Calculate size for the background image
    QSize szi(sz.width() + 2*DISTANCE, sz.height() + 2*DISTANCE);

    QImage tmp(szi, QImage::Format_ARGB32_Premultiplied);
    QPixmap scaled = QPixmap(szi);
    scaled.fill(Qt::white);
    tmp.fill(0);
    QPainter tmpPainter(&tmp);
    tmpPainter.setCompositionMode(QPainter::CompositionMode_Source);
    tmpPainter.drawPixmap(QPointF(-DISTANCE, -DISTANCE), scaled);
    tmpPainter.end();

    // blur the alpha channel
    QImage blurred(tmp.size(), QImage::Format_ARGB32_Premultiplied);
    blurred.fill(0);
    QPainter blurPainter(&blurred);
    qt_blurImage(&blurPainter, tmp, dbRadius, false, true);
    blurPainter.end();

    tmp = blurred;

    // blacken the image...
    tmpPainter.begin(&tmp);
    tmpPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    tmpPainter.fillRect(tmp.rect(), QColor(0, 0, 0, 80));
    tmpPainter.end();

    // draw the blurred shadow...
    painter->drawImage(offset, tmp);

    QPaintDevice* original_pdev_ptr = painter->device();
    painter->restore();
    painter->end();

    auto model = qobject_cast<const ScTypesModel *>(index.model());
    ScTypesItem itemData;
    itemData.strDescription = model->data(index, ScTypesModel::DescriptionRole).toString();
    itemData.strIcon = model->data(index, Qt::DecorationRole).toString();
    itemData.strType = model->data(index, ScTypesModel::TypeRole).toString();
    ScTypeWidget widget(itemData);

    widget.setGeometry(QRect(offset,sz));
    QWidget* topWidget = QApplication::topLevelAt(option.widget->mapToGlobal(QPoint(0,0)));
    QPoint wgtOffset = option.widget->mapTo(topWidget, QPoint(0,0));
    widget.render(painter->device(),
                  offset + wgtOffset);
    painter->begin(original_pdev_ptr);

}

QSize ScTypesDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index)
    return QSize(option.rect.width(), defHeightItem);
}
