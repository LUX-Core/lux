#include "tabbarinfo.h"
#include <QToolButton>
#include <QSize>

TabBarInfo::TabBarInfo(QStackedWidget *parent) :
    QObject(parent),
    m_current(0),
    m_stack(parent),
    m_tabBar(0),
    m_attached(false),
    m_iconCloseTab(0)
{}

bool TabBarInfo::addTab(int index, const QString &name)
{
    if(m_stack->count() <= index)
    {
        return false;
    }
    m_mapName[index] = name;
    m_mapVisible[index] = true;
    update();
    return true;
}

void TabBarInfo::removeTab(int index)
{
    int count = m_stack->count();
    if(count <= index)
    {
        return;
    }

    QMap<int, QString> mapName;
    QMap<int, bool> mapVisible;
    for(int i = 0; i < count; i++)
    {
        if(i < index)
        {
            mapName[i] = m_mapName[i];
            mapVisible[i] = m_mapVisible[i];
        }
        else if(i > index)
        {
            mapName[i-1] = m_mapName[i];
            mapVisible[i-1] = m_mapVisible[i];
        }
    }
    m_mapName = mapName;
    m_mapVisible = mapVisible;

    QWidget* widget = m_stack->widget(index);
    m_stack->removeWidget(widget);
    widget->deleteLater();

    update();
}

void TabBarInfo::setTabVisible(int index, bool visible)
{
    if(m_stack->count() > index)
    {
        m_mapVisible[index] = visible;
    }
    update();
}

void TabBarInfo::attach(QTabBar *tabBar, QIcon* iconCloseTab)
{
    m_tabBar = tabBar;
    m_iconCloseTab = iconCloseTab;
    if(m_tabBar)
    {
        connect(m_tabBar, SIGNAL(currentChanged(int)), SLOT(on_currentChanged(int)));
    }
    update();
    m_attached = true;
}

void TabBarInfo::detach()
{
    m_attached = false;
    if(m_tabBar)
    {
        disconnect(m_tabBar, 0, 0, 0);
        int count = m_tabBar->count();
        for(int i = count - 1; i >= 0; i--)
        {
            m_tabBar->removeTab(i);
        }
        m_tabBar = 0;
    }
}

void TabBarInfo::setCurrent(int index)
{
    m_current = index;
    update();
}

void TabBarInfo::clear()
{
    for(int i = m_stack->count() - 1; i > 0; i--)
    {
        QWidget* widget = m_stack->widget(i);
        m_stack->removeWidget(widget);
        widget->deleteLater();
    }

    QMap<int, QString> mapName;
    QMap<int, bool> mapVisible;
    mapName[0] = m_mapName[0];
    mapVisible[0] = m_mapVisible[0];
    m_mapName = mapName;
    m_mapVisible = mapVisible;
    m_current = 0;

    update();
}

void TabBarInfo::on_currentChanged(int index)
{
    if(m_attached && index < m_mapTabInfo.keys().size())
    {
        int tab = m_mapTabInfo[index];
        if(tab < m_stack->count())
        {
            m_stack->setCurrentIndex(tab);
        }
        m_current = tab;
    }
}

void TabBarInfo::on_closeButtonClick()
{
    if(m_attached && m_tabBar)
    {
        QObject* obj = sender();
        if(obj)
        {
            for(int index = 0; index < m_tabBar->count(); index++)
            {
                if(obj == m_tabBar->tabButton(index, QTabBar::RightSide))
                {
                    if(index < m_mapTabInfo.keys().size())
                    {
                        int tab = m_mapTabInfo[index];
                        removeTab(tab);
                    }
                    break;
                }
            }
        }
    }
}

void TabBarInfo::update()
{
    if(m_tabBar)
    {
        // Populate the tab bar
        QMap<int, int> mapTabInfo;
        int currentTab = 0;
        int numberTabs = m_mapName.keys().size();
        for(int i = 0; i < numberTabs; i++)
        {
            bool visible = m_mapVisible[i];
            if(visible)
            {
                if(m_tabBar->count() > currentTab)
                {
                    m_tabBar->setTabText(currentTab, m_mapName[i]);
                }
                else
                {
                    m_tabBar->addTab(m_mapName[i]);
                    int count = m_tabBar->count();
                    m_tabBar->setTabButton(count - 1, QTabBar::LeftSide, 0);
                    if(count == 1)
                    {
                        m_tabBar->setTabButton(0, QTabBar::RightSide, 0);
                    }
                    else
                    {
                        if(m_iconCloseTab)
                        {
                            QToolButton* tool = new QToolButton(m_tabBar);
                            tool->setIcon(*m_iconCloseTab);
                            tool->setObjectName("tabBarTool");
                            tool->setFixedSize(16, 16);
                            m_tabBar->setTabButton(count - 1, QTabBar::RightSide, tool);
                            connect(tool, SIGNAL(clicked(bool)), SLOT(on_closeButtonClick()));
                        }
                    }
                }
                mapTabInfo[currentTab] = i;
                currentTab++;
            }
        }
        int count = m_tabBar->count();
        if(currentTab < count)
        {
            for(int i = count - 1; i >= currentTab; i--)
            {
                m_tabBar->removeTab(i);
            }
        }
        m_mapTabInfo = mapTabInfo;

        // Set the current tab
        int tabCurrent = m_mapTabInfo[m_tabBar->currentIndex()];
        if(tabCurrent != m_current)
        {
            for(int i = 0; i < m_tabBar->count(); i++)
            {
                tabCurrent = m_mapTabInfo[i];
                if(tabCurrent == m_current)
                {
                    m_tabBar->setCurrentIndex(tabCurrent);
                }
            }
        }
    }
}
