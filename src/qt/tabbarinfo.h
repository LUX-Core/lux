#ifndef TABBARINFO_H
#define TABBARINFO_H

#include <QStackedWidget>
#include <QMap>
#include <QTabBar>
#include <QIcon>

/**
 * @brief The TabBarInfo class Class for informations about tabs.
 * Useful in case of custom tab widgets, like having tab control inside the title.
 */
class TabBarInfo : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief TabBarInfo Constructor
     * @param parent Parent stack
     */
    explicit TabBarInfo(QStackedWidget *parent);

    /**
     * @brief addTab Add new tab
     * @param index Stack index
     * @param name Tab name
     * @return Success of the operation
     */
    bool addTab(int index, const QString& name);

    /**
     * @brief removeTab Removing tab
     * @param index Index of the tab
     */
    void removeTab(int index);

    /**
     * @brief setTabVisible Set tab visible
     * @param index Stack index
     * @param visible true: visible, false: not visible
     */
    void setTabVisible(int index, bool visible);

    /**
     * @brief attach Attack the tab bar
     * @param tabBar Tab bar control
     * @param iconCloseTab Close tab icon
     */
    void attach(QTabBar* tabBar, QIcon* iconCloseTab);

    /**
     * @brief detach Detach the tab bar
     */
    void detach();

    /**
     * @brief setCurrent Set current index
     * @param index Stack index
     */
    void setCurrent(int index);

    /**
     * @brief clear Remove all tabs except the first one which is not closable
     */
    void clear();

public Q_SLOTS:
    /**
     * @brief on_currentChanged Slot for changing the tab bar index
     * @param index Tab bar index
     */
    void on_currentChanged(int index);

    /**
     * @brief on_closeButtonClick Slot for close button click
     */
    void on_closeButtonClick();

private:
    /**
     * @brief update Update the GUI
     */
    void update();

private:
    int m_current;
    QMap<int, QString> m_mapName;
    QMap<int, bool> m_mapVisible;
    QMap<int, int> m_mapTabInfo;
    QStackedWidget* m_stack;
    QTabBar* m_tabBar;
    bool m_attached;
    QIcon* m_iconCloseTab;
};

#endif // TABBARINFO_H
