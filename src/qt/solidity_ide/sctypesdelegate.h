#ifndef CSTYPESDELEGATE_H
#define CSTYPESDELEGATE_H

#include <QStyledItemDelegate>

class ScTypesDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    ScTypesDelegate(QObject *parent = nullptr);
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;
};

#endif // CSTYPESDELEGATE_H
