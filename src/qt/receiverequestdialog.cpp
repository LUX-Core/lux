// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "receiverequestdialog.h"
#include "ui_receiverequestdialog.h"

#include "bitcoinunits.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "walletmodel.h"

#include <QClipboard>
#include <QDrag>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#if QT_VERSION < 0x050000
#include <QUrl>
#endif

#if defined(HAVE_CONFIG_H)
#include "config/lux-config.h" /* for USE_QRCODE */
#endif

#ifdef USE_QRCODE
#include <qrencode.h>
#endif

QRImageWidget::QRImageWidget(QWidget* parent) : QLabel(parent), contextMenu(0)
{
    contextMenu = new QMenu(this);
    QAction* saveImageAction = new QAction(tr("&Save Image..."), this);
    connect(saveImageAction, SIGNAL(triggered()), this, SLOT(saveImage()));
    contextMenu->addAction(saveImageAction);
    QAction* copyImageAction = new QAction(tr("&Copy Image"), this);
    connect(copyImageAction, SIGNAL(triggered()), this, SLOT(copyImage()));
    contextMenu->addAction(copyImageAction);
}

QImage QRImageWidget::exportImage()
{
    if (!pixmap())
        return QImage();
    return pixmap()->toImage();
}

void QRImageWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && pixmap()) {
        event->accept();
        QMimeData* mimeData = new QMimeData;
        mimeData->setImageData(exportImage());

        QDrag* drag = new QDrag(this);
        drag->setMimeData(mimeData);
        drag->exec();
    } else {
        QLabel::mousePressEvent(event);
    }
}

void QRImageWidget::saveImage()
{
    if (!pixmap())
        return;
    QString fn = GUIUtil::getSaveFileName(this, tr("Save QR Code"), QString(), tr("PNG Image (*.png)"), NULL);
    if (!fn.isEmpty()) {
        exportImage().save(fn);
    }
}

void QRImageWidget::copyImage()
{
    if (!pixmap())
        return;
    QApplication::clipboard()->setImage(exportImage());
}

void QRImageWidget::contextMenuEvent(QContextMenuEvent* event)
{
    if (!pixmap())
        return;
    contextMenu->exec(event->globalPos());
}

ReceiveRequestDialog::ReceiveRequestDialog(QWidget* parent) : QDialog(parent),
                                                              ui(new Ui::ReceiveRequestDialog),
                                                              model(0)
{
    ui->setupUi(this);

#ifndef USE_QRCODE
    ui->btnSaveAs->setVisible(false);
    ui->lblQRCode->setVisible(false);
#endif

    connect(ui->btnSaveAs, SIGNAL(clicked()), ui->lblQRCode, SLOT(saveImage()));
}

ReceiveRequestDialog::~ReceiveRequestDialog()
{
    delete ui;
}

void ReceiveRequestDialog::setModel(OptionsModel* model)
{
    this->model = model;

    if (model)
        connect(model, SIGNAL(displayUnitChanged(int)), this, SLOT(update()));

    // update the display unit if necessary
    update();
}

void ReceiveRequestDialog::setInfo(const SendCoinsRecipient& info)
{
    this->info = info;
    update();
}


bool ReceiveRequestDialog::createQRCode(QLabel *label, SendCoinsRecipient _info)
{
#ifdef USE_QRCODE
    QString uri = GUIUtil::formatBitcoinURI(_info);
    label->setText("");
    if(!uri.isEmpty())
    {
        // limit URI length
        if (uri.length() > MAX_URI_LENGTH)
        {
            label->setText(tr("Resulting URI too long, try to reduce the text for label / message."));
        } else {
            QRcode *code = QRcode_encodeString(uri.toUtf8().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
            if (!code)
            {
                label->setText(tr("Error encoding URI into QR Code."));
                return false;
            }
            QImage qrImage = QImage(code->width + 8, code->width + 8, QImage::Format_RGB32);
            qrImage.fill(0xffffff);
            unsigned char *p = code->data;
            for (int y = 0; y < code->width; y++)
            {
                for (int x = 0; x < code->width; x++)
                {
                    qrImage.setPixel(x + 4, y + 4, ((*p & 1) ? 0x0 : 0xffffff));
                    p++;
                }
            }

            QRcode_free(code);

            QImage qrAddrImage = QImage(EXPORT_IMAGE_SIZE, EXPORT_IMAGE_SIZE+20, QImage::Format_RGB32);
            qrAddrImage.fill(0xffffff);
            QPainter painter(&qrAddrImage);
            painter.drawImage(0, 0, qrImage.scaled(EXPORT_IMAGE_SIZE, EXPORT_IMAGE_SIZE));
            QFont font = GUIUtil::fixedPitchFont();
            font.setPixelSize(12);
            painter.setFont(font);
            QRect paddedRect = qrAddrImage.rect();
            paddedRect.setHeight(EXPORT_IMAGE_SIZE+12);
            painter.drawText(paddedRect, Qt::AlignBottom|Qt::AlignCenter, _info.address);
            painter.end();

            label->setPixmap(QPixmap::fromImage(qrAddrImage));
            return true;
        }
    }
#else
    Q_UNUSED(label);
    Q_UNUSED(_info);
#endif
    return false;
}

void ReceiveRequestDialog::update()
{
    if (!model)
        return;
    QString target = info.label;
    if (target.isEmpty())
        target = info.address;
    setWindowTitle(tr("Request payment to %1").arg(target));

    QString uri = GUIUtil::formatBitcoinURI(info);
    ui->btnSaveAs->setEnabled(false);
    QString html;
    html += "<html><font face='verdana, arial, helvetica, sans-serif'>";
    html += "<b>" + tr("Payment information") + "</b><br>";
    html += "<b>" + tr("URI") + "</b>: ";
    html += "<a style=\"color:#5B4C7C;\" href=\"" + uri + "\">" + GUIUtil::HtmlEscape(uri) + "</a><br>";
    html += "<b>" + tr("Address") + "</b>: " + GUIUtil::HtmlEscape(info.address) + "<br>";
    if (info.amount)
        html += "<b>" + tr("Amount") + "</b>: " + BitcoinUnits::formatWithUnit(model->getDisplayUnit(), info.amount) + "<br>";
    if (!info.label.isEmpty())
        html += "<b>" + tr("Label") + "</b>: " + GUIUtil::HtmlEscape(info.label) + "<br>";
    if (!info.message.isEmpty())
        html += "<b>" + tr("Message") + "</b>: " + GUIUtil::HtmlEscape(info.message) + "<br>";
    ui->outUri->setText(html);

#ifdef USE_QRCODE
    ui->lblQRCode->setText("");
    if (!uri.isEmpty()) {
        // limit URI length
        if (uri.length() > MAX_URI_LENGTH) {
            ui->lblQRCode->setText(tr("Resulting URI too long, try to reduce the text for label / message."));
        } else {
            QRcode* code = QRcode_encodeString(uri.toUtf8().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
            if (!code) {
                ui->lblQRCode->setText(tr("Error encoding URI into QR Code."));
                return;
            }
            QImage qrImage = QImage(code->width + 8, code->width + 8, QImage::Format_RGB32);
            qrImage.fill(0xffffff);
            unsigned char* p = code->data;
            for (int y = 0; y < code->width; y++) {
                for (int x = 0; x < code->width; x++) {
                    qrImage.setPixel(x + 4, y + 4, ((*p & 1) ? 0x0 : 0xffffff));
                    p++;
                }
            }
            QRcode_free(code);

            QImage qrAddrImage = QImage(EXPORT_IMAGE_SIZE, EXPORT_IMAGE_SIZE+20, QImage::Format_RGB32);
            qrAddrImage.fill(0xffffff);
            QPainter painter(&qrAddrImage);
            painter.drawImage(0, 0, qrImage.scaled(EXPORT_IMAGE_SIZE, EXPORT_IMAGE_SIZE));
            QFont font = GUIUtil::fixedPitchFont();
            font.setPixelSize(12);
            painter.setFont(font);
            QRect paddedRect = qrAddrImage.rect();
            paddedRect.setHeight(EXPORT_IMAGE_SIZE+12);
            painter.drawText(paddedRect, Qt::AlignBottom|Qt::AlignCenter, info.address);
            painter.end();

            ui->lblQRCode->setPixmap(QPixmap::fromImage(qrAddrImage));
            ui->btnSaveAs->setEnabled(true);
        }
    }
#endif
}

void ReceiveRequestDialog::on_btnCopyURI_clicked()
{
    GUIUtil::setClipboard(GUIUtil::formatBitcoinURI(info));
}

void ReceiveRequestDialog::on_btnCopyAddress_clicked()
{
    GUIUtil::setClipboard(info.address);
}
