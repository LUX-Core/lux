#ifndef CUSTOMLIST_H
#define CUSTOMLIST_H

#include <QListView>
#include <QWheelEvent>
#include <QMouseEvent>

class CustomList : public QListView {
public:
    CustomList(QWidget* parent): QListView(parent) {}

    void wheelEvent(QWheelEvent* event) override {
        event->ignore();
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        event->ignore();
    }
};

#endif //CUSTOMLIST_H
