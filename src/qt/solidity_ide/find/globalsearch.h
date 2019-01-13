#ifndef GLOBALSEARCH_H
#define GLOBALSEARCH_H

#include <QString>
#include <QVector>
#include <QMap>
struct SearchItem
{
    int numLine;
    QString strLine;
    int start;
    int length;
};
typedef QMap<QString,QVector<SearchItem>> findResults;
#endif // GLOBALSEARCH_H
