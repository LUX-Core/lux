#include "scwizardwgt.h"
#include "ui_scwizardwgt.h"

ScWizardWgt::ScWizardWgt(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::scWizardWgt)
{
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose);
#if defined(WIN32) || defined(WIN64) || defined(__linux__)
    setWindowFlags(Qt::Window);
    setWindowFlag(Qt::WindowMinimizeButtonHint, false);
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);
#endif
#ifdef __APPLE__
    setWindowFlags(Qt::Window|Qt::WindowStaysOnTopHint
                   | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint);
#endif

    ui->stackedWidget->setCurrentWidget(ui->allTypes);
    connect(ui->allTypes, &ScAllTypesWidget::goToToken,
            this, [this](){ui->stackedWidget->setCurrentWidget(ui->tokenProperties);});
}

ScWizardWgt::~ScWizardWgt()
{
    delete ui;
}
