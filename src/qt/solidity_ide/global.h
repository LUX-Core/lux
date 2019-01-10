#ifndef GLOBAL_H
#define GLOBAL_H
#include <QString>

class QWidget;

#define defVersionApplication "0.8.4.1"
#define defVersionEncryptProtocol 2

#define appFontWeight QFont::Normal
#define defColorNoEditable "#f8f8f8"
#define defHashKey "cmpsv"
#define defPathBlockpads "blockpads"
#define defExternalBlockpads "external_blockpads"

#define defNoneId "none"

//settings
#define defCurrentFile "CurrentFile"
#define defReadMeVersion "ReadMeVersion"

#define defSearch "Search"
#define defRegularExpressions defSearch + QString("/RegularExpressions")
#define defCase defSearch + QString("/Case")
#define defWholeWord defSearch + QString("/WholeWord")
#define defWrapAround defSearch + QString("/WrapAround")

#define defCloud "Cloud"
#define defDownloadCloud defCloud + QString("/DownloadPath")
#define defAutoOpen defCloud + QString("/AutoOpen")

//properties qapplication
#define defEmailProperty "email"
#define defPasswordProperty "password"
#define defLicenseProperty "license"
#define defIdProperty "id"
#define defDateStart "date_start"
#define defFileProperty "fileBlockpad"
#define def2FA_Code "2FA_Code"
#define defLicenseIsActive "LicenseIsActive"

namespace Utilities {
    void setAppFamilyFont(  QWidget * wgt,
                            int pointSize,
                            int weight = -1,
                            bool italic = false);
    QString filesDirectory();
    QString applicationPath();
    QString macAddress();
}
#endif // GLOBAL_H
