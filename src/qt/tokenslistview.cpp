#include "tokenslistview.h"
#include <QWheelEvent>

tokensListView::tokensListView(QWidget *parent):
    QListView(parent)
{

}

void tokensListView::wheelEvent(QWheelEvent *e)
{
    e->ignore();
    return;
}
