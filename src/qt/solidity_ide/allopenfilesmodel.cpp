#include "allopenfilesmodel.h"

#define defTmpFileToolTip QString("It is only tmp file. But you can store it in disk if you click \"Save as\".")


AllOpenFilesModel::AllOpenFilesModel(QObject *parent):
    QAbstractListModel(parent)
{

}

int AllOpenFilesModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return items.size();
}

QVariant AllOpenFilesModel::data(const QModelIndex &index, int role) const
{
    if(Qt::DisplayRole == role)
    {
        QString displayData = items[index.row()].fileInfo.fileName();
        if(items[index.row()].bEdit || items[index.row()].bTmp)
            displayData += "*";
        return displayData;
    }
    else if(Qt::ToolTipRole == role)
    {
        if(!items[index.row()].bTmp)
            return items[index.row()].fileInfo.absoluteFilePath();
        else
            return defTmpFileToolTip;
    }
    else if(AllDataRole == role)
    {
        return QVariant::fromValue(items[index.row()]);
    }
    else if(EditingDataRole == role)
    {
        return items[index.row()].bEdit;
    }
    else if(TmpDataRole == role)
    {
        return items[index.row()].bTmp;
    }
    else if(AbsFilePathRole == role)
    {
        return items[index.row()].fileInfo.absoluteFilePath();
    }
    else
        return QVariant();
}

bool AllOpenFilesModel::setData(int iRow, const QVariant &value, int role)
{
    return setData(index(iRow), value, role);
}

bool AllOpenFilesModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if(index.row() >= items.size() || index.row() < 0)
        return QAbstractListModel::setData(index,value,role);

    if(AllDataRole == role)
    {
        items[index.row()] = qvariant_cast<EditFileData>(value);
    }
    else if(EditingDataRole == role)
    {
        items[index.row()].bEdit = value.toBool();
    }
    else if(AbsFilePathRole == role)
    {
        items[index.row()].fileInfo.setFile(value.toString());
    }
    else
        return QAbstractListModel::setData(index,value,role);

    emit dataChanged(index, index, {role});
    return true;
}

bool AllOpenFilesModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if(row < 0 || (row > items.size() && items.size()!= 0))
        return false;
    beginInsertRows(parent,row, row+count-1);
    for(int i=row; i<row+count; i++)
    {
        items.insert(i, EditFileData());
    }
    endInsertRows();
    return true;
}

bool AllOpenFilesModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if(row < 0 || row+count > items.size())
        return false;
    beginRemoveRows(parent,row, row+count-1);
    for(int i=0; i<count; i++)
    {
        items.removeAt(row);
    }
    endRemoveRows();
    return true;
}
