#ifndef HTMLDELEGATE_H
#define HTMLDELEGATE_H

#include <QStyledItemDelegate>
#include <QString>

class QPainter;

class LuxgatePriceDelegate : public QStyledItemDelegate
{
public:
    LuxgatePriceDelegate(QObject *parent = nullptr);
protected:
    void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
    QSize sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const;
private:
    QString formattedText(QString text, bool bBid) const; //if bBid==true price of bid, else price of ask
};

#endif // HTMLDELEGATE_H
