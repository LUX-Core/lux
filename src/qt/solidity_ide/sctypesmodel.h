#ifndef CsTypesMODEL_H
#define CsTypesMODEL_H

#include <QAbstractListModel>

struct ScTypesItem
{
    ScTypesItem(QString strIcon,
                   QString strType,
                   QString strDescription):
                   strIcon(strIcon),
                   strType(strType),
                   strDescription(strDescription)
    {}
    ScTypesItem():
                   strIcon(),
                   strType(),
                   strDescription()
    {}
    QString strIcon;
    QString strType;
    QString strDescription;
};


class ScTypesModel : public QAbstractListModel
{
    Q_OBJECT

public:
    ScTypesModel(QObject *parent = nullptr);

    enum DataRole{
        TypeRole = Qt::UserRole + 1,
        DescriptionRole = Qt::UserRole + 2
    };

    //override functions
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    bool setData(int iRow, const QVariant &value, int role = Qt::EditRole);
private:
    QList<ScTypesItem> items;
};

#endif // CsTypesMODEL_H
