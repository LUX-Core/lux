#ifndef HTMLDELEGATE_H
#define HTMLDELEGATE_H

#include <QStyledItemDelegate>
#include <QString>

class QPainter;

class LuxgateHtmlDelegate : public QStyledItemDelegate
{
public:
    LuxgateHtmlDelegate(QObject *parent = nullptr);
protected:
    void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
    QSize sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const;
};

#endif // HTMLDELEGATE_H
