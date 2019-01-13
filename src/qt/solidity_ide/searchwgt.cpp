#include "searchwgt.h"
#include "ui_searchwgt.h"
#include <QEvent>
#include <QMessageBox>
#include "global.h"
#include <QDebug>
#include <QMetaEnum>
#include <QDesktopWidget>

SearchWgt::SearchWgt(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SearchWgt)
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
    //load settings
    {
        ui->checkBoxCase->setChecked(settings.value(defCase).toBool());
        ui->checkBoxRegularExpressions->setChecked(settings.value(defRegularExpressions).toBool());
        ui->checkBoxWholeWord->setChecked(settings.value(defWholeWord).toBool());
        ui->checkBoxWrapAround->setChecked(settings.value(defWrapAround).toBool());
    }
    ui->comboBoxFindWhat->setFocus();
    slotRegularClicked(ui->checkBoxRegularExpressions->isChecked());
    slotFindRadButClicked();
    //signals-slots connects
    {
        //transparency
        connect(ui->groupBoxTransparency, &QGroupBox::clicked,
                this, &SearchWgt::slotTransparencyGroupClicked);
        connect(ui->horizontalSliderTransparency, &QSlider::valueChanged,
                this, &SearchWgt::slotSliderTransparencyChanged);
        connect(ui->radioButtonLosingFocus, &QRadioButton::clicked,
                this, &SearchWgt::slotOnLosingFocusClicked);
        connect(ui->radioButtonAllways, &QRadioButton::clicked,
                this, &SearchWgt::slotAllwaysClicked);

        //options
        connect(ui->checkBoxRegularExpressions, &QCheckBox::clicked,
                this, &SearchWgt::slotRegularClicked);
        connect(ui->checkBoxCase, &QCheckBox::clicked,
                this, &SearchWgt::slotCaseClicked);
        connect(ui->checkBoxWholeWord, &QCheckBox::clicked,
                this, &SearchWgt::slotWholeWordClicked);
        connect(ui->checkBoxWrapAround, &QCheckBox::clicked,
                this, &SearchWgt::slotWrapAroundClicked);

        //mark
        connect(ui->comboBoxFindWhat, &QComboBox::editTextChanged,
                this, &SearchWgt::sigMark);

        //find
        connect(ui->pushButtonFindAllCurrent, &QPushButton::clicked,
                this, &SearchWgt::slotFindAllCurrent);
        connect(ui->pushButtonFindAllCurProject, &QPushButton::clicked,
                this, &SearchWgt::sigFindAllCurProject);
        connect(ui->pushButtonFindAllAll, &QPushButton::clicked,
                this, &SearchWgt::slotFindAllAll);
        connect(ui->pushButtonFindNext, &QPushButton::clicked,
                this, &SearchWgt::slotFindNext);
        connect(ui->pushButtonFindPrev, &QPushButton::clicked,
                this, &SearchWgt::slotFindPrev);

        //replace
        connect(ui->pushButtonReplace, &QPushButton::clicked,
                this, &SearchWgt::slotReplace);
        connect(ui->pushButtonReplaceAllCurrent, &QPushButton::clicked,
                this, &SearchWgt::slotReplaceAllCurrent);
        connect(ui->pushButtonReplaceAllAll, &QPushButton::clicked,
                this, &SearchWgt::slotReplaceAllAll);
        connect(ui->pushButtonReplaceAllCurProject, &QPushButton::clicked,
                this, &SearchWgt::slotReplaceAllCurProject);

        //clear
        connect(ui->pushButtonClear, &QPushButton::clicked,
                this, &SearchWgt::slotClearMarks);

        //choose find-replace
        connect(ui->radioButtonReplace, &QRadioButton::clicked,
                this, &SearchWgt::slotReplaceRadButClicked);
        connect(ui->radioButtonFind, &QRadioButton::clicked,
                this, &SearchWgt::slotFindRadButClicked);
    }
#ifdef __linux__
    setGeometry(
        QStyle::alignedRect(
            Qt::LeftToRight,
            Qt::AlignCenter,
            QSize(this->size()),
            qApp->desktop()->availableGeometry()
        )
    );
#endif
}

void SearchWgt::slotReplace()
{
    if(ui->comboBoxReplaceWith->findText(ui->comboBoxFindWhat->currentText())<0)
        ui->comboBoxReplaceWith
           ->addItem(ui->comboBoxReplaceWith->currentText());
    emit sigReplace(ui->comboBoxReplaceWith->currentText());
}

void SearchWgt::slotReplaceAllCurrent()
{
    if(ui->comboBoxReplaceWith->findText(ui->comboBoxFindWhat->currentText())<0)
        ui->comboBoxReplaceWith
           ->addItem(ui->comboBoxReplaceWith->currentText());
    emit sigReplaceAllCurrent(ui->comboBoxReplaceWith->currentText());
}

void SearchWgt::slotReplaceAllAll()
{
    if(ui->comboBoxReplaceWith->findText(ui->comboBoxFindWhat->currentText())<0)
        ui->comboBoxReplaceWith
           ->addItem(ui->comboBoxReplaceWith->currentText());
    emit sigReplaceAllAll(ui->comboBoxReplaceWith->currentText());
}

void SearchWgt::slotReplaceAllCurProject()
{
    if(ui->comboBoxReplaceWith->findText(ui->comboBoxFindWhat->currentText())<0)
        ui->comboBoxReplaceWith
           ->addItem(ui->comboBoxReplaceWith->currentText());
    emit sigReplaceAllCurProject(ui->comboBoxReplaceWith->currentText());
}

void SearchWgt::slotFindRadButClicked()
{
    //hide all buttons
    {
        auto pbuttons = findChildren<QPushButton*>();
        foreach(auto pb, pbuttons)
        {
            pb->hide();
        }
    }
    ui->widgetReplaceWith->hide();
    ui->groupBox->setTitle(tr("Find"));
    //show find
    {
        ui->pushButtonFindNext->show();
        ui->pushButtonFindPrev->show();
        ui->pushButtonClear->show();
        ui->pushButtonFindAllAll->show();
        ui->pushButtonFindAllCurrent->show();
        ui->pushButtonFindAllCurProject->show();
    }
}

void SearchWgt::slotReplaceRadButClicked()
{
    //hide all buttons
    {
        auto pbuttons = findChildren<QPushButton*>();
        foreach(auto pb, pbuttons)
        {
            pb->hide();
        }
    }
    ui->widgetReplaceWith->show();
    ui->groupBox->setTitle(tr("Replace"));
    //show replace
    {
        ui->pushButtonFindNext->show();
        ui->pushButtonFindPrev->show();
        ui->pushButtonReplace->show();
        ui->pushButtonReplaceAllAll->show();
        ui->pushButtonReplaceAllCurrent->show();
        ui->pushButtonReplaceAllCurProject->show();
    }
}

void SearchWgt::slotClearMarks()
{
    ui->comboBoxFindWhat->setEditText("");
}

void SearchWgt::slotFindNext()
{
    if(ui->comboBoxFindWhat->currentText().isEmpty())
    {
        QMessageBox::critical(nullptr, "Search",
                              "Please input text in \"Find what\" field");
    }
    else
    {
        if(ui->comboBoxFindWhat->findText(ui->comboBoxFindWhat->currentText())<0)
            ui->comboBoxFindWhat->addItem(ui->comboBoxFindWhat->currentText());
        emit sigFindNext();
    }
}

void SearchWgt::slotFindPrev()
{
    if(ui->comboBoxFindWhat->currentText().isEmpty())
    {
        QMessageBox::critical(nullptr, "Search",
                              "Please input text in \"Find what\" field");
    }
    else
    {
        if(ui->comboBoxFindWhat->findText(ui->comboBoxFindWhat->currentText())<0)
            ui->comboBoxFindWhat->addItem(ui->comboBoxFindWhat->currentText());
        emit sigFindPrev();
    }
}

void SearchWgt::slotFindAllCurProject()
{
    if(!ui->comboBoxFindWhat->currentText().isEmpty())
    {
        emit sigFindAllCurProject();
        if(ui->comboBoxFindWhat->findText(ui->comboBoxFindWhat->currentText())<0)
            ui->comboBoxFindWhat->addItem(ui->comboBoxFindWhat->currentText());
    }
    else
    {
        QMessageBox::critical(nullptr, "Search",
                              "Please input text in \"Find what\" field");
    }
}

void SearchWgt::slotFindAllAll()
{
    if(!ui->comboBoxFindWhat->currentText().isEmpty())
    {
        emit sigFindAllAllFiles();
        if(ui->comboBoxFindWhat->findText(ui->comboBoxFindWhat->currentText())<0)
            ui->comboBoxFindWhat->addItem(ui->comboBoxFindWhat->currentText());
    }
    else
    {
        QMessageBox::critical(nullptr, "Search",
                              "Please input text in \"Find what\" field");
    }
}

void SearchWgt::slotWrapAroundClicked(bool checked)
{
    settings.setValue(defWrapAround, checked);
    settings.sync();
}

bool SearchWgt::event(QEvent *e)
{
    // window was activated
    if (e->type() == QEvent::WindowActivate)
    {
        qDebug() << "WindowActivate";
        if(ui->radioButtonLosingFocus->isChecked()
                ||
           !ui->groupBoxTransparency->isChecked())
        {
            setWindowOpacity(1.0);
        }
        else
        {
            if(ui->horizontalSliderTransparency->value()
                    <10)
                setWindowOpacity(0.1);          
        }
    }
    // window was deactivated
    if (e->type() == QEvent::WindowDeactivate)
    {
        qDebug() << "WindowDeActivate";
        if(ui->radioButtonLosingFocus->isChecked())
        {
            setWindowOpacity(0.01 *
                             (qreal)ui->horizontalSliderTransparency->value());
        }
    }
    return QWidget::event(e);
}

void SearchWgt::slotRegularClicked(bool checked)
{
    if(checked)
        ui->checkBoxWholeWord->setEnabled(false);
    else
        ui->checkBoxWholeWord->setEnabled(true);

    settings.setValue(defRegularExpressions, checked);
    settings.sync();
    emit sigMark(ui->comboBoxFindWhat->currentText());
}

void SearchWgt::slotCaseClicked(bool checked)
{
    settings.setValue(defCase, checked);
    settings.sync();
    emit sigMark(ui->comboBoxFindWhat->currentText());
}

void SearchWgt::slotWholeWordClicked(bool checked)
{
    settings.setValue(defWholeWord, checked);
    settings.sync();
    emit sigMark(ui->comboBoxFindWhat->currentText());
}

void SearchWgt::slotFindAllCurrent()
{
    if(!ui->comboBoxFindWhat->currentText().isEmpty())
    {
        emit sigFindAllCurrentFile();
        if(ui->comboBoxFindWhat->findText(ui->comboBoxFindWhat->currentText())<0)
            ui->comboBoxFindWhat->addItem(ui->comboBoxFindWhat->currentText());
    }
    else
    {
        QMessageBox::critical(nullptr, "Search",
                              "Please input text in \"Find what\" field");
    }
}
void SearchWgt::slotOnLosingFocusClicked()
{
    setWindowOpacity(1.0);
}

void SearchWgt::slotAllwaysClicked()
{
    setWindowOpacity(0.01 *
                     (qreal)ui->horizontalSliderTransparency->value());
}

void SearchWgt::slotSliderTransparencyChanged(int value)
{
    if(ui->radioButtonAllways->isChecked()
            &&
       ui->groupBoxTransparency->isChecked())
    {
        setWindowOpacity(0.01 * (qreal)value);
    }
}

void SearchWgt::slotTransparencyGroupClicked(bool checked)
{
    if(checked)
    {
        if(ui->radioButtonAllways->isChecked())
        {
            setWindowOpacity(0.01 *
                             (qreal)ui->horizontalSliderTransparency->value());
        }
    }
    else
    {
        setWindowOpacity(1.0);
    }
}

SearchWgt::~SearchWgt()
{
    delete ui;
}
