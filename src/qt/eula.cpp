// Copyright (c) 2015-2018 The LUX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "eula.h"
#include "ui_eula.h"
#include "masternode.h"
#include "clientversion.h"

#include <QSettings>
#include <QCloseEvent>

Eula::Eula(QWidget* parent) : QDialog(parent),
                              ui(new Ui::Eula),
                              isRemembered(false)
{
    ui->setupUi(this);
    
    //Set fix size window
    this->setFixedSize(this->width(),this->height());

    // Remove minimize button
    this->setWindowFlags(Qt::Dialog);

    QCoreApplication::setApplicationName(tr("End User Software License Agreement"));

    QString headerInfo = tr("<p style=\"line-height:140\"><span><br>Please read the following license agreement. You must accept the terms contained in this agreement before continuing with the application.");
    ui->header->setTextFormat(Qt::RichText);
    ui->header->setAlignment(Qt::AlignJustify|Qt::AlignTop);
    ui->header->setText(headerInfo);

    ui->radAccept->setText(tr("I have read and agree to the terms contained in the license agreements."));
    ui->radAccept->setChecked(true);
    ui->radNonAccept->setText(tr("I do not accept the terms and conditions of above licence agreements."));
    ui->checkBox->setText(tr("Don't show this dialog again"));

    QString eulaInfo = tr("");
    eulaInfo += tr("<p style=\"line-height:10\"><b>GENERAL</b></p>");
    eulaInfo += tr("<p style=\"line-height:10\">---------------------</p>");
    eulaInfo += tr("<p style=\"line-height:30\"><br></p>");
    eulaInfo += tr("<p style=\"line-height:130\">PLEASE READ THIS CONTRACT \
                   CAREFULLY. BY USING ALL OR ANY PORTION OF THE SOFTWARE YOU ACCEPT ALL THE \
                   TERMS AND CONDITIONS OF THIS AGREEMENT, INCLUDING, IN PARTICULAR THE \
                   LIMITATIONS ON: USE CONTAINED IN SECTION 2; TRANSFERABILITY IN SECTION \
                   4; WARRANTY IN SECTION 6 AND 7; AND LIABILITY IN SECTION 8. YOU AGREE \
                   THAT THIS AGREEMENT IS ENFORCEABLE LIKE ANY WRITTEN NEGOTIATED AGREEMENT \
                   SIGNED BY YOU. IF YOU DO NOT AGREE, DO NOT USE THIS SOFTWARE. IF YOU ACQUIRED \
                   THE SOFTWARE ON TANGIBLE MEDIA (e.g. CD) WITHOUT AN OPPORTUNITY TO REVIEW THIS \
                   LICENSE AND YOU DO NOT ACCEPT THIS AGREEMENT, YOU MAY OBTAIN A REFUND OF THE \
                   AMOUNT YOU ORIGINALLY PAID IF YOU: (A) DO NOT USE THE SOFTWARE AND (B) RETURN IT, \
                   WITH PROOF OF PAYMENT, TO THE LOCATION FROM WHICH IT WAS OBTAINED WITHIN THIRTY \
                   (30) DAYS OF THE PURCHASE DATE.</p>");

    eulaInfo += tr("<p style=\"line-height:130\"><br><b>1.	Definitions</b></p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">When used in this Agreement, the following terms \
                   shall have the respective meanings indicated, such meanings to be applicable to \
                   both the singular and plural forms of the terms defined: </p>");

    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><b>\"Licensor\"</b> means Luxcore, with its main address \
                   located at Suite 3 Level 27, Governor Macquarie Tower, 1 Farrer Place, Sydney, NSW, 2000.</p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><b>\"Software\"</b> means (a) all of the contents of the files, \
                   disk(s), CD-ROM(s) or other media with which this Agreement is provided, including but not \
                   limited to (i) Luxcore or third party computer information or software; (ii) digital images, \
                   stock photographs, clip art, sounds or other artistic works (\"Stock Files\"); (iii) related \
                   explanatory written materials or files (\"Documentation\"); and (iv) fonts; and (b) upgrades, \
                   modified versions, updates, additions, and copies of the Software, if any, licensed to you by \
                   Luxcore (collectively, \"Updates\"). </p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><b>\"Use\"</b> or <b>\"Using\"</b> means to access, install, download, \
                   copy or otherwise benefit from using the functionality of the Software in accordance with the \
                   Documentation. </p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><b>\"Licensee\"</b> means You or Your Company, unless otherwise indicated.</p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><b>\"Permitted Number\"</b> means one (1) unless otherwise indicated under \
                   a valid license (e.g. volume license) granted by Luxcore.</p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><b>\"Computer\"</b> means an electronic device that accepts information in \
                   digital or similar form and manipulates it for a specific result based on a sequence of instructions.</p>");

    eulaInfo += tr("<p style=\"line-height:130\"><br><b>2.	Software License</b></p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">As long as you comply with the terms of this End User License Agreement (the \
                   \"Agreement\"), Luxcore grants to you a non-exclusive license to Use the Software for the purposes described \
                    in the Documentation. Some third party materials included in the Software may be subject to other terms and \
                    conditions, which are typically found in a \"Read Me\" file located near such materials.</p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><br><b>2.1	General Use</b></p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">You may install and Use a copy of the Software on your compatible computer, \
                   up to the Permitted Number of computers; or</p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><br><b>2.2	Server Use</b></p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">You may install one copy of the Software on your computer file server for the \
                   purpose of downloading and installing the Software onto other computers within your internal network up to \
                   the Permitted Number or you may install one copy of the Software on a computer file server within your internal \
                   network for the sole and exclusive purpose of using the Software through commands, data or instructions (e.g. \
                   scripts) from an unlimited number of computers on your internal network. No other network use is permitted, \
                   including but not limited to, using the Software either directly or through commands, data or instructions from \
                   or to a computer not part of your internal network, for internet or web hosting services or by any user not licensed \
                   to use this copy of the Software through a valid license from Luxcore; and</p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><br><b>2.3	Backup Copy</b></p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">You may make one backup copy of the Software, provided your backup copy is not installed \
                    or used on any computer. You may not transfer the rights to a backup copy unless you transfer all rights in the Software \
                    as provided under Section 6.</p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><br><b>2.4	Home Use </b></p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">You, as the primary user of the computer on which the Software is installed, may also install \
                   the Software on one of your home computers. However, the Software may not be used on your home computer at the same time the \
                   Software on the primary computer is being used.</p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><br><b>2.5	Stock Files</b></p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">Unless stated otherwise in the \"Read-Me\" files associated with the Stock Files, which may \
                   include specific rights and restrictions with respect to such materials, you may display, modify, reproduce and distribute \
                   any of the Stock Files included with the Software. However, you may not distribute the Stock Files on a stand-alone basis, \
                   i.e., in circumstances in which the Stock Files constitute the primary value of the product being distributed. Stock Files \
                   may not be used in the production of libelous, defamatory, fraudulent, lewd, obscene or pornographic material or any material \
                   that infringes upon any third party intellectual property rights or in any otherwise illegal manner. You may not claim any \
                   trademark rights in the Stock Files or derivative works thereof.</p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><br><b>2.6	Limitations</b></p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">To the extent that the Software includes Luxcore Luxgate software, (i) you may customize the \
                   installer for such software in accordance with the restrictions found at <a href=\"https://luxcore.io\">https://luxcore.io</a> \
                   (e.g., installation of additional plug-in and help files); however, you may not otherwise alter or modify the installer program \
                   or create a new installer for any of such software, (ii) such software is licensed and distributed by Luxcore, and (iii) you are \
                   not authorized to use any plug-in or enhancement that permits you to save modifications to a any format file with such software; \
                   however, such use is authorized with Luxcore, Luxcore Luxgate, and other current and future Luxcore products. For information on \
                   how to distribute Luxgate please refer to the sections entitled \"How to Distribute Luxgate\" at \
                   <a href=\"https://luxcore.io\">https://luxcore.io</a>.</p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><br><b>3.	Intellectual Property Rights</b></p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">The Software and any copies that you are authorized by Luxcore to make are the intellectual property \
                    of and are owned by Luxcore and its suppliers. The structure, organization and code of the Software are the valuable trade secrets \
                    and confidential information of Luxcore and its suppliers. The Software is protected by copyright, including without limitation by \
                    Australia Copyright Law, international treaty provisions and applicable laws in the country in which it is being used. You may not \
                    copy the Software, except as set forth in Section 2 (\"Software License\").</p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">Any copies that you are permitted to make pursuant to this Agreement must contain the same copyright \
                   and other proprietary notices that appear on or in the Software. You also agree not to reverse engineer, decompile, disassemble or \
                   otherwise attempt to discover the source code of the Software except to the extent you may be expressly permitted to decompile under \
                   applicable law, it is essential to do so in order to achieve operability of the Software with another software program, and you have \
                   first requested Luxcore to provide the information necessary to achieve such operability and Luxcore has not made such information \
                   available.</p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">Luxcore has the right to impose reasonable conditions and to request a reasonable fee before providing \
                    such information. Any information supplied by Luxcore or obtained by you, as permitted hereunder, may only be used by you for the \
                    purpose described herein and may not be disclosed to any third party or used to create any software which is substantially similar \
                    to the expression of the Software. Requests for information should be directed to the Luxcore Customer Support Department. Trademarks \
                    shall be used in accordance with accepted trademark practice, including identification of trademarks owners' names. Trademarks can only \
                    be used to identify printed output produced by the Software and such use of any trademark does not give you any rights of ownership in \
                    that trademark. Except as expressly stated above, this Agreement does not grant you any intellectual property rights in the Software.</p>");

    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><br><b>4.	Transfer</b></p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">You may not, rent, lease, sublicense or authorize all or any portion of the Software to be copied onto \
                    another users computer except as may be expressly permitted herein. You may, however, transfer all your rights to Use the Software \
                    to another person or legal entity provided that: (a) you also transfer each this Agreement, the Software and all other software or \
                    hardware bundled or pre-installed with the Software, including all copies, Updates and prior versions, and all copies of font software \
                    converted into other formats, to such person or entity; (b) you retain no copies, including backups and copies stored on a computer; \
                    and (c) the receiving party accepts the terms and conditions of this Agreement and any other terms and conditions upon which you legally \
                    purchased a license to the Software. Notwithstanding the foregoing, you may not transfer education, pre-release, or not for resale copies \
                    of the Software.</p>");

    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><br><b>5.	Multiple Environment Software / Multiple Language Software / Dual Media Software / Multiple \
                    Copies/ Bundles / Updates</b></p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">If the Software supports multiple platforms or languages, if you receive the Software on multiple media, if \
                    you otherwise receive multiple copies of the Software, or if you received the Software bundled with other software, the total number of \
                    your computers on which all versions of the Software are installed may not exceed the Permitted Number. You may not, rent, lease, sublicense, \
                    lend or transfer any versions or copies of such Software you do not Use. If the Software is an Update to a previous version of the Software, \
                    you must possess a valid license to such previous version in order to Use the Update. You may continue to Use the previous version of the Software \
                    on your computer after you receive the Update to assist you in the transition to the Update, provided that: the Update and the previous version \
                    are installed on the same computer; the previous version or copies thereof are not transferred to another party or computer unless all copies of \
                    the Update are also transferred to such party or computer; and you acknowledge that any obligation Luxcore may have to support the previous version \
                    of the Software may be ended upon availability of the Update.</p>");

    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><br><b>6.	NO WARRANTY</b></p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">The Software is being delivered to you \"AS IS\" and Luxcore makes no warranty as to its use or performance. Luxcore AND \
                    ITS SUPPLIERS DO NOT AND CANNOT WARRANT THE PERFORMANCE OR RESULTS YOU MAY OBTAIN BY USING THE SOFTWARE. EXCEPT FOR ANY WARRANTY, CONDITION, \
                    REPRESENTATION OR TERM TO THE EXTENT TO WHICH THE SAME CANNOT OR MAY NOT BE EXCLUDED OR LIMITED BY LAW APPLICABLE TO YOU IN YOUR JURISDICTION, Luxcore \
                    AND ITS SUPPLIERS MAKE NO WARRANTIES CONDITIONS, REPRESENTATIONS, OR TERMS (EXPRESS OR IMPLIED WHETHER BY STATUTE, COMMON LAW, CUSTOM, USAGE OR OTHERWISE) \
                    AS TO ANY MATTER INCLUDING WITHOUT LIMITATION NONINFRINGEMENT OF THIRD PARTY RIGHTS, MERCHANTABILITY, INTEGRATION, SATISFACTORY QUALITY, OR FITNESS FOR ANY \
                    PARTICULAR PURPOSE.</p>");

    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><br><b>7.	Pre-release Product Additional Terms</b></p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">If the product you have received with this license is pre-commercial release or beta Software (\"Pre-release Software\"), then \
                    the following Section applies. To the extent that any provision in this Section is in conflict with any other term or condition in this Agreement, this \
                    Section shall supercede such other term(s) and condition(s) with respect to the Pre-release Software, but only to the extent necessary to resolve the conflict. \
                    You acknowledge that the Software is a pre-release version, does not represent final product from Luxcore, and may contain bugs, errors and other problems that \
                    could cause system or other failures and data loss. Consequently, the Pre-release Software is provided to you \"AS-IS\", and Luxcore disclaims any warranty or \
                    liability obligations to you of any kind. WHERE LEGALLY LIABILITY CANNOT BE EXCLUDED FOR PRE-RELEASE SOFTWARE, BUT IT MAY BE LIMITED, LUXCORE'S LIABILITY AND \
                    THAT OF ITS SUPPLIERS SHALL BE LIMITED TO THE SUM OF FIFTY DOLLARS (U.S. $50) IN TOTAL. You acknowledge that Luxcore has not promised or guaranteed to you that \
                    Pre-release Software will be announced or made available to anyone in the future, that Luxcore has no express or implied obligation to you to announce or \
                    introduce the Pre-release Software and that Luxcore may not introduce a product similar to or compatible with the Pre-release Software. Accordingly, you \
                    acknowledge that any research or development that you perform regarding the Pre-release Software or any product associated with the Pre-release Software is \
                    done entirely at your own risk. During the term of this Agreement, if requested by Luxcore, you will provide feedback to Luxcore regarding testing and use of \
                    the Pre-release Software, including error or bug reports. If you have been provided the Pre-release Software pursuant to a separate written agreement, such as \
                    the Luxcore Serial Agreement for Unreleased Products, your use of the Software is also governed by such agreement. You agree that you may not and certify that \
                    you will not sublicense, lease, loan, rent, or transfer the Pre-release Software. Upon receipt of a later unreleased version of the Pre-release Software or \
                    release by Luxcore of a publicly released commercial version of the Software, whether as a stand-alone product or as part of a larger product, you agree to \
                    return or destroy all earlier Pre-release Software received from Luxcore and to abide by the terms of the End User License Agreement for any such later versions \
                    of the Pre-release Software. Notwithstanding anything in this Section to the contrary, if you are located outside the United States of America or Canada, you agree \
                    that you will return or destroy all unreleased versions of the Pre-release Software within thirty (30) days of the completion of your testing of the Software \
                    when such date is earlier than the date for Luxcore's first commercial shipment of the publicly released (commercial) Software.</p>");


    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><br><b>8.	LIMITATION OF LIABILITY</b></p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">IN NO EVENT WILL LUXCORE OR ITS SUPPLIERS BE LIABLE TO YOU FOR ANY DAMAGES, CLAIMS OR COSTS WHATSOEVER OR ANY CONSEQUENTIAL, INDIRECT, \
                   INCIDENTAL DAMAGES, OR ANY LOST PROFITS OR LOST SAVINGS, EVEN IF AN LUXCORE REPRESENTATIVE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH LOSS, DAMAGES, CLAIMS OR \
                   COSTS OR FOR ANY CLAIM BY ANY THIRD PARTY. THE FOREGOING LIMITATIONS AND EXCLUSIONS APPLY TO THE EXTENT PERMITTED BY APPLICABLE LAW IN YOUR JURISDICTION. LUXCORE'S \
                   AGGREGATE LIABILITY AND THAT OF ITS SUPPLIERS UNDER OR IN CONNECTION WITH THIS AGREEMENT SHALL BE LIMITED TO THE AMOUNT PAID FOR THE SOFTWARE, IF ANY. Nothing \
                   contained in this Agreement limits Luxcore's liability to you in the event of death or personal injury resulting from Luxcore's negligence or for the tort of \
                   deceit (fraud). Luxcore is acting on behalf of its suppliers for the purpose of disclaiming, excluding and/or limiting obligations, warranties and liability as \
                   provided in this Agreement, but in no other respects and for no other purpose. For further information, please see the jurisdiction specific information at the \
                   end of this Agreement, if any, or contact Luxcore's Customer Support Department.</p>");

    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><br><b>9.	Export Rules (OPTIONAL - FOR AMERICAN COMPANIES)</b></p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">You agree that the Software will not be shipped, transferred or exported into any country or used in any manner prohibited by the \
                   United States Export Administration Act or any other export laws, restrictions or regulations (collectively the \"Export Laws\"). In addition, if the Software is \
                   identified as export controlled items under the Export Laws, you represent and warrant that you are not a citizen, or otherwise located within, an embargoed nation \
                   (including without limitation Iran, Iraq, Syria, Sudan, Libya, Cuba, North Korea, and Serbia) and that you are not otherwise prohibited under the Export Laws from \
                   receiving the Software. All rights to Use the Software are granted on condition that such rights are forfeited if you fail to comply with the terms of this \
                   Agreement.</p>");

    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><br><b>10.	Governing Law</b></p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">This Agreement shall be governed by and interpreted in accordance with the laws of the Melbourne, Victoria, Australia.</p>");

    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><br><b>11.	General Provisions</b></p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">If any part of this Agreement is found void and unenforceable, it will not affect the validity of the balance of the Agreement, \
                   which shall remain valid and enforceable according to its terms. This Agreement shall not prejudice the statutory rights of any party dealing as a consumer. \
                   This Agreement may only be modified by a writing signed by an authorized officer of Luxcore. Updates may be licensed to you by Luxcore with additional or \
                   different terms. This is the entire agreement between Luxcore and you relating to the Software and it supersedes any prior representations, discussions, \
                   undertakings, communications or advertising relating to the Software.</p>");

    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><br><b>12.	Notice to U.S. Government End Users</b></p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">The Software and Documentation are \"Commercial Items,\" as that term is defined at 48 C.F.R. ") + QString::fromUtf8("§") +
                tr ("2.101, consisting of \"Commercial Computer Software\" and \"Commercial Computer Software Documentation,\" as such terms are used in 48 C.F.R. ") +
                QString::fromUtf8("§") + tr("12.212 or 48 C.F.R. ") + QString::fromUtf8("§") + tr("227.7202, as applicable. Consistent with 48 C.F.R. ") +
                QString::fromUtf8("§") + tr("12.212 or 48 C.F.R. ") + QString::fromUtf8("§") + QString::fromUtf8("§") + tr("227.7202-1 through 227.7202-4, as applicable, the \
                Commercial Computer Software and Commercial Computer Software Documentation are being licensed to U.S. Government end users (a) only as Commercial Items and \
                (b) with only those rights as are granted to all other end users pursuant to the terms and conditions herein. Unpublished-rights reserved under the copyright \
                laws of the United States. For U.S. Government End Users, Luxcore agrees to comply with all applicable equal opportunity laws including, if appropriate, the \
                provisions of Executive Order 11246, as amended, Section 402 of the Vietnam Era Veterans Readjustment Assistance Act of 1974 (38 USC 4212), and Section 503 \
                of the Rehabilitation Act of 1973, as amended, and the regulations at 41 CFR Parts 60-1 through 60-60, 60-250, and 60-741. The affirmative action clause and \
                regulations contained in the preceding sentence shall be incorporated by reference in this Agreement.</p>");

    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\"><br><b>13.	Compliance with Licenses</b></p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">If you are a business or organization, you agree that upon request from Luxcore or Luxcore 's authorized representative, you will \
                   within thirty (30) days fully document and certify that use of any and all Luxcore Software at the time of the request is in conformity with your valid licenses \
                   from Luxcore.</p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">If you have any questions regarding this Agreement or if you wish to request any information from Luxcore please use the address \
                    and contact information included with this product to contact the Luxcore office serving your jurisdiction.</p>");
    eulaInfo += tr("<p style=\"line-height:30\"></p>");
    eulaInfo += tr("<p style=\"line-height:130\">Luxcore, Luxgate, and all other Luxcore's products are either registered trademarks or trademarks of Luxcore in the United States, \
                    Europe and/or other countries.</p><br><br>");


    QLabel* eulaLabel = new QLabel;
    eulaLabel->setTextFormat(Qt::RichText);
    eulaLabel->setWordWrap(true);
    eulaLabel->setText(eulaInfo);
    eulaLabel->setAlignment(Qt::AlignJustify|Qt::AlignTop);

    ui->scrollArea->setWidget(eulaLabel);
}

Eula::~Eula()
{
    delete ui;
}

void Eula::on_cancel_clicked()
{
    exit(0);
}

void Eula::on_next_clicked()
{
    close();
}

void Eula::showDialog()
{
    bool isDialogHiding = false;
    QSettings settings;

    QString currentVersion = tr("lux_") + QString::fromStdString(strprintf("%d%d%d%d",
                             CLIENT_VERSION_MAJOR,
                             CLIENT_VERSION_MINOR,
                             CLIENT_VERSION_REVISION,
                             CLIENT_VERSION_BUILD
                             ));

    if (settings.contains(tr("luxVersion")))
    {
        QString storeVersion = settings.value(tr("luxVersion")).toString();
        if (QString::compare(storeVersion, currentVersion, Qt::CaseInsensitive) == 0)
        {
            isDialogHiding = settings.value(storeVersion).toBool();
        }
        else
        {
            settings.remove(storeVersion);
            settings.setValue(tr("luxVersion"), currentVersion);
        }
    }
    else
    {
        settings.setValue(tr("luxVersion"), currentVersion);
    }
	
    if(isDialogHiding)
    {
        return;
    }

    Eula eula;
    eula.setWindowIcon(QIcon(":icons/bitcoin"));
    eula.exec();

    settings.setValue(currentVersion, eula.isEulaRemembered());
}


void Eula::closeEvent (QCloseEvent *event)
{
    if (ui->radAccept->isChecked())
    {
        isRemembered = ui->checkBox->isChecked();
    }
    else
    {
        exit(0);
    }
}

bool Eula::isEulaRemembered()
{
    return isRemembered;
}


