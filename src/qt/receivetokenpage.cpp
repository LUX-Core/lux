#include "receivetokenpage.h"
#include "ui_receivetokenpage.h"
#include "guiconstants.h"
#include "receiverequestdialog.h"
#include "guiutil.h"

#include <QSettings>

ReceiveTokenPage::ReceiveTokenPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ReceiveTokenPage)
{
    ui->setupUi(this);
	
		QSettings settings;
	
	if (settings.value("theme").toString() == "dark grey") {
		QString styleSheet = ".QPushButton { background-color: #262626; color:#fff; border: 1px solid #fff5f5; "
								"padding-left:10px; padding-right:10px; min-height:25px; min-width:75px; }"
								".QPushButton:hover { background-color:#fff5f5 !important; color:#262626 !important; }";					
		ui->copyAddressButton->setStyleSheet(styleSheet);
	} else if (settings.value("theme").toString() == "dark blue") {
		QString styleSheet = ".QPushButton { background-color: #031d54; color:#fff; border: 1px solid #fff5f5; "
								"padding-left:10px; padding-right:10px; min-height:25px; min-width:75px; }"
								".QPushButton:hover { background-color:#fff5f5; color:#031d54; }";					
		ui->copyAddressButton->setStyleSheet(styleSheet);
	} else { 
		//code here
	}
	
    connect(ui->copyAddressButton, SIGNAL(clicked()), this, SLOT(on_copyAddressClicked()));
}

ReceiveTokenPage::~ReceiveTokenPage()
{
    delete ui;
}

void ReceiveTokenPage::setAddress(QString address)
{
    m_address = address;
    createQRCode();
}

void ReceiveTokenPage::setSymbol(QString symbol)
{
    QString addressText = symbol.isEmpty() ? "" : (QString("%1 ").arg(symbol) + tr("Address"));
}

void ReceiveTokenPage::on_copyAddressClicked()
{
    if(!m_address.isEmpty())
        GUIUtil::setClipboard(m_address);
}

void ReceiveTokenPage::createQRCode()
{
    SendCoinsRecipient info;
    if(!m_address.isEmpty())
    {
        info.address = m_address;
        if(ReceiveRequestDialog::createQRCode(ui->lblQRCode, info))
        {
            ui->lblQRCode->setVisible(true);
            ui->lblQRCode->setScaledContents(true);
        }
        else
        {
            ui->lblQRCode->setVisible(false);
        }
        ui->labelTokenAddress->setText(m_address);
        ui->copyAddressButton->setVisible(true);
    }
    else
    {
        ui->lblQRCode->clear();
        ui->labelTokenAddress->setText("");
        ui->copyAddressButton->setVisible(false);
    }
}
