#ifndef TOKENSLISTVIEW_H
#define TOKENSLISTVIEW_H

#include <QListView>

class tokensListView : public QListView
{
public:
    tokensListView(QWidget *parent = nullptr);
protected:
    void wheelEvent(QWheelEvent *e);
};

#endif // TOKENSLISTVIEW_H
