// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The LUX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_GUICONSTANTS_H
#define BITCOIN_QT_GUICONSTANTS_H

/* Milliseconds between model updates */
static const int MODEL_UPDATE_DELAY = 1000;

/* AskPassphraseDialog -- Maximum passphrase length */
static const int MAX_PASSPHRASE_SIZE = 1024;

/* Lux GUI -- Size of icons in status bar */
static const int STATUSBAR_ICONSIZE = 16;

static const bool DEFAULT_SPLASHSCREEN = true;

/* Invalid field background style */
#define STYLE_INVALID "background:#FF8080"
/* Transaction list -- unconfirmed transaction */
#define COLOR_UNCONFIRMED QColor(128, 128, 128)
/* Transaction list -- negative amount */
#define COLOR_NEGATIVE QColor(255, 0, 0)
/* Transaction list -- bare address (without label) */
#define COLOR_BAREADDRESS QColor(140, 140, 140)
/* Transaction list -- TX status decoration - open until date */
#define COLOR_TX_STATUS_OPENUNTILDATE QColor(64, 64, 255)
/* Transaction list -- TX status decoration - default color */
#define COLOR_BLACK QColor(51, 51, 51)
#define COLOR_TX_STATUS_DANGER QColor(200, 100, 100)

/* Transaction list -- unconfirmed transaction when a darker theme is selected */
#define SECOND_COLOR_UNCONFIRMED QColor(255, 255, 255)
/* Transaction list -- negative amount when a darker theme is selected */
#define SECOND_COLOR_NEGATIVE QColor(255, 125, 125)
/* Transaction list -- bare address (without label) when a darker theme is selected */
#define SECOND_COLOR_BAREADDRESS QColor(255, 255, 255)
/* Transaction list -- TX status decoration - default color when a darker theme is selected */
#define SECOND_COLOR_WHITE QColor(255, 255, 255)

#define CONFIRM_ICONS 5

/* Tooltips longer than this (in characters) are converted into rich text,
   so that they can be word-wrapped.
 */
static const int TOOLTIP_WRAP_THRESHOLD = 80;

/* Maximum allowed URI length */
static const int MAX_URI_LENGTH = 255;

/* QRCodeDialog -- size of exported QR Code image */
#define EXPORT_IMAGE_SIZE 256

/* Number of frames in spinner animation */
#define SPINNER_FRAMES 35

#define QAPP_ORG_NAME "LUX"
#define QAPP_ORG_DOMAIN "luxcore.io"
#define QAPP_APP_NAME_DEFAULT "LUX-Qt"
#define QAPP_APP_NAME_TESTNET "LUX-Qt-testnet"

#endif // BITCOIN_QT_GUICONSTANTS_H
