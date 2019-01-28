#include "luxgatehistorymodel.h"

LuxgateHistoryModel::LuxgateHistoryModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

QVariant LuxgateHistoryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    // FIXME: Implement me!
}

int LuxgateHistoryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    // FIXME: Implement me!
}

int LuxgateHistoryModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    // FIXME: Implement me!
}

QVariant LuxgateHistoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    // FIXME: Implement me!
    return QVariant();
}
