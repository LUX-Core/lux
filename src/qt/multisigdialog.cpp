// Copyright (c) 2017 The LUX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "multisigdialog.h"
#include "forms/ui_multisigdialog.h"

#include "askpassphrasedialog.h"
#include "primitives/transaction.h"
#include "addressbookpage.h"
#include "utilstrencodings.h"
#include "core_io.h"
#include "script/script.h"
#include "base58.h"
#include "coins.h"
#include "keystore.h"
#include "init.h"
#include "wallet.h"
#include "script/sign.h"
#include "script/interpreter.h"
#include "utilmoneystr.h"
#include "guiutil.h"
#include "qvalidatedlineedit.h"
#include "bitcoinamountfield.h"

#include <QtCore/QVariant>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QClipboard>


MultisigDialog::MultisigDialog(QWidget* parent) : QDialog(parent),
                                                  ui(new Ui::MultisigDialog),
                                                  model(0)
{
    ui->setupUi(this);
    multisigTx = CMutableTransaction();

    //flag to show keyScrollArea on first priv key added
    isFirstPrivKey = true;
    isFirstRawTx = true;
    ui->keyScrollArea->hide();
    ui->txInputsScrollArea->hide();

    connect(ui->commitButton, SIGNAL(clicked()), this, SLOT(commitMultisigTx()));

    //populate lists with initial objects
    on_addAddressButton_clicked();
    on_addAddressButton_clicked();
    on_addDestinationButton_clicked();

    this->setStyleSheet(GUIUtil::loadStyleSheet());
}

MultisigDialog::~MultisigDialog()
{
    delete ui;
}

void MultisigDialog::setModel(WalletModel *model)
{
    this->model = model;
}

void MultisigDialog::showTab(int index)
{
    ui->multisigTabWidget->setCurrentIndex(index);
    this->show();
}

void MultisigDialog::updateCoinControl(CAmount nAmount, unsigned int nQuantity)
{
    ui->labelAmount_int->setText(QString::fromStdString(FormatMoney(nAmount)));
    ui->labelQuantity_int->setText(QString::number(nQuantity));
}

/**
* Private Slots
*/
//slot for pasting addresses
void MultisigDialog::pasteText()
{
    QWidget* pasteButton = qobject_cast<QWidget*>(sender());
    if(!pasteButton)return;

    QFrame* addressFrame = qobject_cast<QFrame*>(pasteButton->parentWidget());
    if(!addressFrame)return;

    QValidatedLineEdit* vle = addressFrame->findChild<QValidatedLineEdit*>("address");
    if(!vle)return;

    vle->setText(QApplication::clipboard()->text());
}

//slot for deleting QFrames with the delete buttons
void MultisigDialog::deleteFrame()
{
   QWidget *buttonWidget = qobject_cast<QWidget*>(sender());
   if(!buttonWidget)return;

   //if deleting last raw input/priv key, hide scroll area
   if(buttonWidget->objectName() == "inputDeleteButton" && ui->inputsList->count() == 1){
       isFirstRawTx = true;
       ui->txInputsScrollArea->hide();
   }else if(buttonWidget->objectName() == "keyDeleteButton" && ui->keyList->count() == 1){
       isFirstPrivKey = true;
       ui->keyScrollArea->hide();
   }

   QFrame* frame = qobject_cast<QFrame*>(buttonWidget->parentWidget());
   if(!frame)return;

   delete frame;
}

//slot to open address book dialog
void MultisigDialog::addressBookButtonReceiving()
{
    QWidget* addressButton = qobject_cast<QWidget*>(sender());
    if(!addressButton)return;

    QFrame* addressFrame = qobject_cast<QFrame*>(addressButton->parentWidget());
    if(!addressFrame)return;

    QValidatedLineEdit* vle = addressFrame->findChild<QValidatedLineEdit*>("address");
    if(!vle)return;

    if (model && model->getAddressTableModel()) {
        AddressBookPage dlg(AddressBookPage::ForSelection, AddressBookPage::ReceivingTab, this);
        dlg.setModel(model->getAddressTableModel());
        if (dlg.exec()) {
             vle->setText(dlg.getReturnValue());
        }
    }
}

//create address
void MultisigDialog::on_addMultisigButton_clicked()
{
    if(!model)
        return;

    int m = ui->enterMSpinbox->value();

    vector<string> keys;

    for (int i = 0; i < ui->addressList->count(); i++) {
        QWidget* address = qobject_cast<QWidget*>(ui->addressList->itemAt(i)->widget());
        QValidatedLineEdit* vle = address->findChild<QValidatedLineEdit*>("address");

        if(!vle->text().isEmpty()){
            keys.push_back(vle->text().toStdString());
        }
    }

    addMultisig(m, keys);
}

void MultisigDialog::on_importAddressButton_clicked(){
    if(!model)
        return;

    string sRedeem = ui->importRedeem->text().toStdString();

    if(sRedeem.empty()){
        ui->addMultisigStatus->setStyleSheet("QLabel { color: red; }");
        ui->addMultisigStatus->setText("Import box empty!");
        return;
    }

    vector<string> vRedeem;
    size_t pos = 0;

    //search redeem input delimited by space
    while ((pos = sRedeem.find(" ")) != std::string::npos) {
        vRedeem.push_back(sRedeem.substr(0, pos));
        sRedeem.erase(0, pos + 1);
    }

    vector<string> keys(vRedeem.begin()+1, vRedeem.end()-1);

    addMultisig(stoi(vRedeem[0]), keys);

    // rescan to find txs associated with imported address
    pwalletMain->ScanForWalletTransactions(chainActive.Genesis(), true);
    pwalletMain->ReacceptWalletTransactions();
}

bool MultisigDialog::addMultisig(int m, vector<string> keys){
    try{
        string error;
        CScript redeem;

        if(!createRedeemScript(m, keys, redeem, error)){
            throw runtime_error(error.data());
        }

        if (::IsMine(*pwalletMain, redeem) == ISMINE_SPENDABLE){
            throw runtime_error("The wallet already contains this script");
        }

        if(!pwalletMain->AddCScript(redeem)){
            throw runtime_error("Failure: address invalid or already exists");
        }

        CScriptID innerID(redeem);
        string label = ui->multisigAddressLabel->text().toStdString();
        pwalletMain->SetAddressBook(innerID, label, "receive");
        if (!pwalletMain->AddMultiSig(redeem)){
            throw runtime_error("Failure: unable to add address as watch only");
        }

        ui->addMultisigStatus->setStyleSheet("QLabel { color: black; }");
        ui->addMultisigStatus->setText("Multisignature address " +
                                       QString::fromStdString(CBitcoinAddress(innerID).ToString()) +
                                       " has been added to the wallet.\nSend the redeem below for other owners to import:\n" +
                                       QString::fromStdString(redeem.ToString()));
    }catch(const runtime_error& e) {
        ui->addMultisigStatus->setStyleSheet("QLabel { color: red; }");
        ui->addMultisigStatus->setText(tr(e.what()));
        return false;
    }
    return true;
}


//spend
void MultisigDialog::on_createButton_clicked()
{
    if(!model)
        return;

    vector<CTxIn> vUserIn;
    vector<CTxOut> vUserOut;
    try{
        //Add inputs from Coin Control if any are selected
        if (CoinControlDialog::coinControl->HasSelected()) {
            vector<COutPoint> vSelected;
            CoinControlDialog::coinControl->ListSelected(vSelected);
            for (auto outpoint : vSelected)
                vUserIn.emplace_back(CTxIn(outpoint));
        }else{//check for raw inputs
            for(int i = 0; i < ui->inputsList->count(); i++){
                QWidget* input = qobject_cast<QWidget*>(ui->inputsList->itemAt(i)->widget());
                QLineEdit* txIdLine = input->findChild<QLineEdit*>("txInputId");
                if(txIdLine->text().isEmpty()){
                    ui->createButtonStatus->setStyleSheet("QLabel { color: red; }");
                    ui->createButtonStatus->setText(tr("Invalid Tx Hash."));
                    return;
                }

                QSpinBox* txVoutLine = input->findChild<QSpinBox*>("txInputVout");
                int nOutput = txVoutLine->value();
                if(nOutput < 0){
                    ui->createButtonStatus->setStyleSheet("QLabel { color: red; }");
                    ui->createButtonStatus->setText(tr("Vout position must be positive."));
                    return;
                }

                uint256 txid = uint256S(txIdLine->text().toStdString());
                CTxIn in(COutPoint(txid, nOutput));
                vUserIn.emplace_back(in);
            }
        }

        //validate destinations
        bool validInput = true;
        for(int i = 0; i < ui->destinationsList->count(); i++){
            QWidget* dest = qobject_cast<QWidget*>(ui->destinationsList->itemAt(i)->widget());
            QValidatedLineEdit* addr = dest->findChild<QValidatedLineEdit*>("destinationAddress");
            BitcoinAmountField* amt = dest->findChild<BitcoinAmountField*>("destinationAmount");
            CBitcoinAddress address;

            bool validDest = true;

            if(!model->validateAddress(addr->text())){
                addr->setValid(false);
                validDest = false;
            }else{
                address = CBitcoinAddress(addr->text().toStdString());
            }

            if(!amt->validate()){
                amt->setValid(false);
                validDest = false;
            }

            if(!validDest){
                validInput = false;
                continue;
            }

            CScript scriptPubKey = GetScriptForDestination(address.Get());
            CTxOut out(amt->value(), scriptPubKey);
            vUserOut.push_back(out);
        }


        //if all user data valid create a multisig tx
        if(validInput){
            //clear member variable
            multisigTx = CMutableTransaction();

            string error;
            string fee;
            if(!createMultisigTransaction(vUserIn, vUserOut, fee, error)){
                throw runtime_error(error);
            }   

            //display status string
            ui->createButtonStatus->setStyleSheet("QTextEdit{ color: black }");

            QString status(strprintf("Transaction has successfully created with a fee of %s.\n"
                                     "The transaction has been automatically imported to the sign tab.\n"
                                     "Please continue on to sign the tx from this wallet, to access the hex to send to other owners.", fee).c_str());

            ui->createButtonStatus->setText(status);
            ui->transactionHex->setText(QString::fromStdString(EncodeHexTx(multisigTx)));

        }
    }catch(const runtime_error& e){
        ui->createButtonStatus->setStyleSheet("QTextEdit{ color: red }");
        ui->createButtonStatus->setText(tr(e.what()));
    }
}

bool MultisigDialog::createMultisigTransaction(vector<CTxIn> vUserIn, vector<CTxOut> vUserOut, string& feeStringRet, string& errorRet)
{
    try{
        //attempt to access the given inputs
        CCoinsViewCache view = getInputsCoinsViewCache(vUserIn);

        //retrieve total input val and change dest
        CAmount totalIn = 0;
        vector<CAmount> vInputVals;
        CScript changePubKey;
        bool fFirst = true;

        for(CTxIn in : vUserIn){
            const CCoins* coins = view.AccessCoins(in.prevout.hash);
            if(!coins->IsAvailable(in.prevout.n) || coins == NULL){
                continue;
            }
            CTxOut prevout = coins->vout[in.prevout.n];
            CScript privKey = prevout.scriptPubKey;

            vInputVals.push_back(prevout.nValue);
            totalIn += prevout.nValue;

            if(!fFirst){
                if(privKey != changePubKey){
                    throw runtime_error("Address mismatch! Inputs must originate from the same multisignature address.");
                }
            }else{
                fFirst = false;
                changePubKey = privKey;
            }
        }

        CAmount totalOut = 0;

        //retrieve total output val
        for(CTxOut out : vUserOut){
            totalOut += out.nValue;
        }

        if(totalIn < totalOut){
            throw runtime_error("Not enough PIV provided as input to complete transaction (including fee).");
        }

        //calculate change amount
        CAmount changeAmount = totalIn - totalOut;
        CTxOut change(changeAmount, changePubKey);

        //generate random position for change
        unsigned int changeIndex = rand() % (vUserOut.size() + 1);

        //insert change into random position
        if(changeIndex < vUserOut.size()){
            vUserOut.insert(vUserOut.begin() + changeIndex, change);
        }else{
            vUserOut.emplace_back(change);
        }

        //populate tx
        CMutableTransaction tx;
        tx.vin = vUserIn;
        tx.vout = vUserOut;

        const CCoins* coins = view.AccessCoins(tx.vin[0].prevout.hash);

        if(coins == NULL || !coins->IsAvailable(tx.vin[0].prevout.n)){
            throw runtime_error("Coins unavailable (unconfirmed/spent)");
        }

        CScript prevPubKey = coins->vout[tx.vin[0].prevout.n].scriptPubKey;

        //get payment destination
        CTxDestination address;
        if(!ExtractDestination(prevPubKey, address)){
            throw runtime_error("Could not find address for destination.");
        }

        CScriptID hash = boost::get<CScriptID>(address);
        CScript redeemScript;

        if (!pwalletMain->GetCScript(hash, redeemScript)){
            throw runtime_error("could not redeem");
        }
        txnouttype type;
        vector<CTxDestination> addresses;
        int nReq;
        if(!ExtractDestinations(redeemScript, type, addresses, nReq)){
            throw runtime_error("Could not extract destinations from redeem script.");
        }

        for(CTxIn& in : tx.vin){
            in.scriptSig.clear();
            //scale estimate to account for multisig scriptSig
            for(unsigned int i = 0; i < 50*(nReq+addresses.size()); i++){
                in.scriptSig << INT64_MAX;
            }
        }

        //calculate fee
        unsigned int nBytes = tx.GetSerializeSize(SER_NETWORK, PROTOCOL_VERSION);
        CAmount fee = ::minRelayTxFee.GetFee(nBytes);

        if(tx.vout.at(changeIndex).nValue > fee){
           tx.vout.at(changeIndex).nValue -= fee;
           feeStringRet = strprintf("%d",((double)fee)/COIN).c_str();
        }else{
            throw runtime_error("Not enough PIV provided to cover fee");
        }

        //clear junk from script sigs
        for(CTxIn& in : tx.vin){
            in.scriptSig.clear();
        }
        multisigTx = tx;
    }catch(const runtime_error& e){
        errorRet = e.what();
        return false;
    }
    return true;
}

//sign
void MultisigDialog::on_signButton_clicked()
{
    if(!model)
        return;
   try{
        //parse tx hex
        CTransaction txRead;
        if(!DecodeHexTx(txRead, ui->transactionHex->text().toStdString())){
            throw runtime_error("Failed to decode transaction hex!");
        }

        CMutableTransaction tx(txRead);

        //check if transaction is already fully verified
        if(isFullyVerified(tx)){
            this->multisigTx = tx;
            ui->commitButton->setEnabled(true);
            ui->signButtonStatus->setText("This transaction is ready to commit. \nThe commit button in now enabled.");
            return;
        }

        string errorOut = string();
        bool fComplete = signMultisigTx(tx, errorOut, ui->keyList);

        if(!errorOut.empty()){
            throw runtime_error(errorOut.data());
        }else{
            this->multisigTx = tx;
        }

        ui->signButtonStatus->setStyleSheet("QTextEdit{ color: black }");
        ui->signButtonStatus->setText(buildMultisigTxStatusString(fComplete, tx));

    }catch(const runtime_error& e){
        ui->signButtonStatus->setStyleSheet("QTextEdit{ color: red }");
        ui->signButtonStatus->setText(tr(e.what()));
    }
}

/***
 *private helper functions
 */
QString MultisigDialog::buildMultisigTxStatusString(bool fComplete, const CMutableTransaction& tx)
{
    string sTxHex = EncodeHexTx(tx);

    if(fComplete){
        ui->commitButton->setEnabled(true);
        string sTxId = tx.GetHash().GetHex();
        string sTxComplete   =  "Complete: true!\n"
                                "The commit button has now been enabled for you to finalize the transaction.\n"
                                "Once the commit button is clicked, the transaction will be published and coins transferred "
                                "to their destinations.\nWARNING: THE ACTIONS OF THE COMMIT BUTTON ARE FINAL AND CANNOT BE REVERSED.";

        return QString(strprintf("%s\nTx Id:\n%s\nTx Hex:\n%s",sTxComplete, sTxId, sTxHex).c_str());
    } else {
        string sTxIncomplete = "Complete: false.\n"
                                "You may now send the hex below to another owner to sign.\n"
                                "Keep in mind the transaction must be passed from one owner to the next for signing.\n"
                                "Ensure all owners have imported the redeem before trying to sign. (besides creator)";

        return QString(strprintf("%s\nTx Hex: %s", sTxIncomplete, sTxHex).c_str());
    }
}


CCoinsViewCache MultisigDialog::getInputsCoinsViewCache(const vector<CTxIn>& vin)
{
    CCoinsView viewDummy;
    CCoinsViewCache view(&viewDummy);
    {
        LOCK(mempool.cs);
        CCoinsViewCache& viewChain = *pcoinsTip;
        CCoinsViewMemPool viewMempool(&viewChain, mempool);
        view.SetBackend(viewMempool); // temporarily switch cache backend to db+mempool view

        for(const CTxIn& txin : vin) {
            const uint256& prevHash = txin.prevout.hash;
            view.AccessCoins(prevHash); // this is certainly allowed to fail
        }

        view.SetBackend(viewDummy); // switch back to avoid locking mempool for too long
    }

    return view;
}


bool MultisigDialog::signMultisigTx(CMutableTransaction& tx, string& errorOut, QVBoxLayout* keyList)
{
    //will be set false if all inputs are not fully signed(valid)
    bool fComplete = true;

    //if keyslist is not default value AND has items in list then true
    bool fGivenKeys = (keyList != nullptr) && (keyList->count() > 0);

    try{

        //copy of vin for reference before vin is mutated
        vector<CTxIn> oldVin(tx.vin);
        CBasicKeyStore privKeystore;

        //if keys were given, attempt to collect redeem and scriptpubkey
        if(fGivenKeys){
            for(int i = 0; i < keyList->count(); i++){
                QWidget* keyFrame = qobject_cast<QWidget*>(keyList->itemAt(i)->widget());
                QLineEdit* key = keyFrame->findChild<QLineEdit*>("key");
                CBitcoinSecret vchSecret;
                if (!vchSecret.SetString(key->text().toStdString()))
                    throw runtime_error("Invalid private key");
                CKey cKey = vchSecret.GetKey();
                if (!cKey.IsValid())
                    throw runtime_error("Private key outside allowed range");
                privKeystore.AddKey(cKey);
            }

            for(CTxIn& txin : tx.vin){
                //get inputs
                CTransaction txVin;
                uint256 hashBlock;
                if (!GetTransaction(txin.prevout.hash, txVin, hashBlock, true))
                    throw runtime_error("txin could not be found");

                if (hashBlock == 0)
                    throw runtime_error("txin is unconfirmed");

                //get pubkey from input
                CScript prevPubKey = txVin.vout[txin.prevout.n].scriptPubKey;

                //get payment destination
                CTxDestination address;
                if(!ExtractDestination(prevPubKey, address)){
                    throw runtime_error("Could not find address for destination.");
                }

                //get redeem script related to destination
                CScriptID hash = boost::get<CScriptID>(address);
                CScript redeemScript;

                if (!pwalletMain->GetCScript(hash, redeemScript)){
                    errorOut = "could not redeem";
                }
                privKeystore.AddCScript(redeemScript);
            }
        }else{
            if (model->getEncryptionStatus() == model->Locked) {
                if (!model->requestUnlock(true).isValid()) {
                    // Unlock wallet was cancelled
                    throw runtime_error("Error: Your wallet is locked. Please enter the wallet passphrase first.");
                }
            }
        }

        //choose between local wallet and provided
        const CKeyStore& keystore = fGivenKeys ? privKeystore : *pwalletMain;

        //attempt to sign each input from local wallet
        int nIn = 0;
        for(CTxIn& txin : tx.vin){
            //get inputs
            CTransaction txVin;
            uint256 hashBlock;
            if (!GetTransaction(txin.prevout.hash, txVin, hashBlock, true))
                throw runtime_error("txin could not be found");

            if (hashBlock == 0)
                throw runtime_error("txin is unconfirmed");

            txin.scriptSig.clear();
            CScript prevPubKey = txVin.vout[txin.prevout.n].scriptPubKey;

            //sign what we can
            SignSignature(keystore, prevPubKey, tx, nIn);

            //merge in any previous signatures
            txin.scriptSig = CombineSignatures(prevPubKey, tx, nIn, txin.scriptSig, oldVin[nIn].scriptSig);

            if (!VerifyScript(txin.scriptSig, prevPubKey, STANDARD_SCRIPT_VERIFY_FLAGS, MutableTransactionSignatureChecker(&tx, nIn))){
                fComplete = false;
            }
            nIn++;
        }

        ui->signButtonStatus->setText(buildMultisigTxStatusString(fComplete, tx));

    }catch(const runtime_error& e){
        errorOut = string(e.what());
        fComplete = false;
    }
    return fComplete;
}

// quick check for an already fully signed tx
bool MultisigDialog::isFullyVerified(CMutableTransaction& tx){
    try{
        int nIn = 0;
        for(CTxIn& txin : tx.vin){
            CTransaction txVin;
            uint256 hashBlock;
            if (!GetTransaction(txin.prevout.hash, txVin, hashBlock, true)){
                throw runtime_error("txin could not be found");
            }
            if (hashBlock == 0){
                throw runtime_error("txin is unconfirmed");
            }

            //get pubkey from this input as output in last tx
            CScript prevPubKey = txVin.vout[txin.prevout.n].scriptPubKey;

            if (!VerifyScript(txin.scriptSig, prevPubKey, STANDARD_SCRIPT_VERIFY_FLAGS, MutableTransactionSignatureChecker(&tx, nIn))){
                return false;
            }

            nIn++;
        }
    }catch(const runtime_error& e){
        return false;
    }

    return true;
}

void MultisigDialog::commitMultisigTx()
{
    CMutableTransaction tx(multisigTx);
    try{
#ifdef ENABLE_WALLET
        CWalletTx wtx(pwalletMain, tx);
        CReserveKey keyChange(pwalletMain);
        if (!pwalletMain->CommitTransaction(wtx, keyChange))
            throw runtime_error(string("Transaction rejected - Failed to commit"));
#else
        uint256 hashTx = tx.GetHash();
        CCoinsViewCache& view = *pcoinsTip;
        const CCoins* existingCoins = view.AccessCoins(hashTx);
        bool fOverrideFees = false;
        bool fHaveMempool = mempool.exists(hashTx);
        bool fHaveChain = existingCoins && existingCoins->nHeight < 1000000000;

        if (!fHaveMempool && !fHaveChain) {
            // push to local node and sync with wallets
            CValidationState state;
            if (!AcceptToMemoryPool(mempool, state, tx, false, NULL, !fOverrideFees)) {
                if (state.IsInvalid())
                    throw runtime_error(strprintf("Transaction rejected - %i: %s", state.GetRejectCode(), state.GetRejectReason()));
                else
                    throw runtime_error(string("Transaction rejected - ") + state.GetRejectReason());
            }
        } else if (fHaveChain) {
            throw runtime_error("transaction already in block chain");
        }
        RelayTransaction(tx);
#endif
        //disable commit if successfully committed
        ui->commitButton->setEnabled(false);
        ui->signButtonStatus->setText(strprintf("Transaction has been successfully published with transaction ID:\n %s", tx.GetHash().GetHex()).c_str());
    }catch(const runtime_error& e){
        ui->signButtonStatus->setText(e.what());
    }
}

bool MultisigDialog::createRedeemScript(int m, vector<string> vKeys, CScript& redeemRet, string& errorRet)
{
    try{
        int n = vKeys.size();
        //gather pub keys
        if (n < 1)
            throw runtime_error("a Multisignature address must require at least one key to redeem");
        if (n < m)
            throw runtime_error(
                strprintf("not enough keys supplied "
                          "(got %d keys, but need at least %d to redeem)",
                    m, n));
        if (n > 15)
            throw runtime_error("Number of addresses involved in the Multisignature address creation > 15\nReduce the number");

        vector<CPubKey> pubkeys;
        pubkeys.resize(n);

        int i = 0;
        for(vector<string>::iterator it = vKeys.begin(); it != vKeys.end(); ++it) {
            string keyString = *it;
    #ifdef ENABLE_WALLET
            // Case 1: LUX address and we have full public key:
            CBitcoinAddress address(keyString);
            if (pwalletMain && address.IsValid()) {
                CKeyID keyID;
                if (!address.GetKeyID(keyID)) {
                    throw runtime_error(
                        strprintf("%s does not refer to a key", keyString));
                }
                CPubKey vchPubKey;
                if (!pwalletMain->GetPubKey(keyID, vchPubKey))
                    throw runtime_error(
                        strprintf("no full public key for address %s", keyString));
                if (!vchPubKey.IsFullyValid()){
                    string sKey = keyString.empty()?"(empty)":keyString;
                    throw runtime_error(" Invalid public key: " + sKey );
                }
                pubkeys[i++] = vchPubKey;
            }

            //case 2: hex pub key
            else
    #endif
            if (IsHex(keyString)) {
                CPubKey vchPubKey(ParseHex(keyString));
                if (!vchPubKey.IsFullyValid()){
                    throw runtime_error(" Invalid public key: " + keyString);
                }
                pubkeys[i++] = vchPubKey;
            } else {
                throw runtime_error(" Invalid public key: " + keyString);
            }
        }
        //populate redeem script
        //OP_N for required signatures
        redeemRet << redeemRet.EncodeOP_N(m);
        //public keys
        for(CPubKey& key : pubkeys){
            vector<unsigned char> vKey= ToByteVector(key);
            redeemRet << vKey;
        }
        //OP_N for total pubkeys
        redeemRet << redeemRet.EncodeOP_N(pubkeys.size());
        redeemRet << OP_CHECKMULTISIG;
        return true;
    }catch(const runtime_error& e){
        errorRet = string(e.what());
        return false;
    }
}

/***
 * Begin QFrame object creation methods
 */
//creates an address object on the create tab
void MultisigDialog::on_addAddressButton_clicked()
{
    //max addresses 15
    if(ui->addressList->count() > 14){
        ui->addMultisigStatus->setStyleSheet("QLabel { color: red; }");
        ui->addMultisigStatus->setText(tr("Maximum possible addresses reached. (16)"));
        return;
    }

    QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    QFrame* addressFrame = new QFrame();
    sizePolicy.setHeightForWidth(addressFrame->sizePolicy().hasHeightForWidth());
    addressFrame->setSizePolicy(sizePolicy);
    addressFrame->setFrameShape(QFrame::StyledPanel);
    addressFrame->setFrameShadow(QFrame::Raised);
    addressFrame->setObjectName(QStringLiteral("addressFrame"));

    QVBoxLayout* frameLayout = new QVBoxLayout(addressFrame);
    frameLayout->setSpacing(1);
    frameLayout->setObjectName(QStringLiteral("frameLayout"));
    frameLayout->setContentsMargins(6, 6, 6, 6);

    QHBoxLayout* addressLayout = new QHBoxLayout();
    addressLayout->setSpacing(0);
    addressLayout->setObjectName(QStringLiteral("addressLayout"));

    QLabel* addressLabel = new QLabel(addressFrame);
    addressLabel->setObjectName(QStringLiteral("addressLabel"));
    addressLabel->setText(QApplication::translate("MultisigDialog", strprintf("Address / Key %i:", ui->addressList->count()+1).c_str() , 0));
    addressLayout->addWidget(addressLabel);

    QValidatedLineEdit* address = new QValidatedLineEdit(addressFrame);
    address->setObjectName(QStringLiteral("address"));
    addressLayout->addWidget(address);

    QPushButton* addressBookButton = new QPushButton(addressFrame);
    addressBookButton->setObjectName(QStringLiteral("addressBookButton"));
    QIcon icon3;
    icon3.addFile(QStringLiteral(":/icons/address-book"), QSize(), QIcon::Normal, QIcon::Off);
    addressBookButton->setIcon(icon3);
    addressBookButton->setAutoDefault(false);
    connect(addressBookButton, SIGNAL(clicked()), this, SLOT(addressBookButtonReceiving()));

    addressLayout->addWidget(addressBookButton);

    QPushButton* addressPasteButton = new QPushButton(addressFrame);
    addressPasteButton->setObjectName(QStringLiteral("addressPasteButton"));
    QIcon icon4;
    icon4.addFile(QStringLiteral(":/icons/editpaste"), QSize(), QIcon::Normal, QIcon::Off);
    addressPasteButton->setIcon(icon4);
    addressPasteButton->setAutoDefault(false);
    connect(addressPasteButton, SIGNAL(clicked()), this, SLOT(pasteText()));

    addressLayout->addWidget(addressPasteButton);

    QPushButton* addressDeleteButton = new QPushButton(addressFrame);
    addressDeleteButton->setObjectName(QStringLiteral("addressDeleteButton"));
    QIcon icon5;
    icon5.addFile(QStringLiteral(":/icons/remove"), QSize(), QIcon::Normal, QIcon::Off);
    addressDeleteButton->setIcon(icon5);
    addressDeleteButton->setAutoDefault(false);
    connect(addressDeleteButton, SIGNAL(clicked()), this, SLOT(deleteFrame()));

    addressLayout->addWidget(addressDeleteButton);
    frameLayout->addLayout(addressLayout);

    ui->addressList->addWidget(addressFrame);
}

void MultisigDialog::on_pushButtonCoinControl_clicked()
{
    CoinControlDialog coinControlDialog(this, true);
    coinControlDialog.setModel(model);
    coinControlDialog.exec();
}

void MultisigDialog::on_addInputButton_clicked()
{
    if(isFirstRawTx){
        isFirstRawTx = false;
        ui->txInputsScrollArea->show();
    }
    QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);

    QFrame* txInputFrame = new QFrame(ui->txInputsWidget);
    sizePolicy.setHeightForWidth(txInputFrame->sizePolicy().hasHeightForWidth());
    txInputFrame->setFrameShape(QFrame::StyledPanel);
    txInputFrame->setFrameShadow(QFrame::Raised);
    txInputFrame->setObjectName(QStringLiteral("txInputFrame"));

    QVBoxLayout* frameLayout = new QVBoxLayout(txInputFrame);
    frameLayout->setSpacing(1);
    frameLayout->setObjectName(QStringLiteral("txInputFrameLayout"));
    frameLayout->setContentsMargins(6, 6, 6, 6);

    QHBoxLayout* txInputLayout = new QHBoxLayout();
    txInputLayout->setObjectName(QStringLiteral("txInputLayout"));

    QLabel* txInputIdLabel = new QLabel(txInputFrame);
    txInputIdLabel->setObjectName(QStringLiteral("txInputIdLabel"));
    txInputIdLabel->setText(QApplication::translate("MultisigDialog", strprintf("%i. Tx Hash: ", ui->inputsList->count()+1).c_str(), 0));
    txInputLayout->addWidget(txInputIdLabel);

    QLineEdit* txInputId = new QLineEdit(txInputFrame);
    txInputId->setObjectName(QStringLiteral("txInputId"));

    txInputLayout->addWidget(txInputId);

    QSpacerItem* horizontalSpacer = new QSpacerItem(10, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
    txInputLayout->addItem(horizontalSpacer);

    QLabel* txInputVoutLabel = new QLabel(txInputFrame);
    txInputVoutLabel->setObjectName(QStringLiteral("txInputVoutLabel"));
    txInputVoutLabel->setText(QApplication::translate("MultisigDialog", "Vout Position: ", 0));

    txInputLayout->addWidget(txInputVoutLabel);

    QSpinBox* txInputVout = new QSpinBox(txInputFrame);
    txInputVout->setObjectName("txInputVout");
    sizePolicy.setHeightForWidth(txInputVout->sizePolicy().hasHeightForWidth());
    txInputVout->setSizePolicy(sizePolicy);
    txInputLayout->addWidget(txInputVout);

    QPushButton* inputDeleteButton = new QPushButton(txInputFrame);
    inputDeleteButton->setObjectName(QStringLiteral("inputDeleteButton"));
    QIcon icon;
    icon.addFile(QStringLiteral(":/icons/remove"), QSize(), QIcon::Normal, QIcon::Off);
    inputDeleteButton->setIcon(icon);
    inputDeleteButton->setAutoDefault(false);
    connect(inputDeleteButton, SIGNAL(clicked()), this, SLOT(deleteFrame()));
    txInputLayout->addWidget(inputDeleteButton);

    frameLayout->addLayout(txInputLayout);

    ui->inputsList->addWidget(txInputFrame);
}

void MultisigDialog::on_addDestinationButton_clicked()
{
    QFrame* destinationFrame = new QFrame(ui->destinationsScrollAreaContents);
    destinationFrame->setObjectName(QStringLiteral("destinationFrame"));
    destinationFrame->setFrameShape(QFrame::StyledPanel);
    destinationFrame->setFrameShadow(QFrame::Raised);

    QVBoxLayout* frameLayout = new QVBoxLayout(destinationFrame);
    frameLayout->setObjectName(QStringLiteral("destinationFrameLayout"));
    QHBoxLayout* destinationLayout = new QHBoxLayout();
    destinationLayout->setSpacing(0);
    destinationLayout->setObjectName(QStringLiteral("destinationLayout"));
    QLabel* destinationAddressLabel = new QLabel(destinationFrame);
    destinationAddressLabel->setObjectName(QStringLiteral("destinationAddressLabel"));

    destinationLayout->addWidget(destinationAddressLabel);

    QValidatedLineEdit* destinationAddress = new QValidatedLineEdit(destinationFrame);
    destinationAddress->setObjectName(QStringLiteral("destinationAddress"));

    destinationLayout->addWidget(destinationAddress);

    QSpacerItem* horizontalSpacer = new QSpacerItem(10, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
    destinationLayout->addItem(horizontalSpacer);

    QLabel* destinationAmountLabel = new QLabel(destinationFrame);
    destinationAmountLabel->setObjectName(QStringLiteral("destinationAmountLabel"));

    destinationLayout->addWidget(destinationAmountLabel);

    BitcoinAmountField* destinationAmount = new BitcoinAmountField(destinationFrame);
    destinationAmount->setObjectName(QStringLiteral("destinationAmount"));

    destinationAddressLabel->setText(QApplication::translate("MultisigDialog", strprintf("%i. Address: ", ui->destinationsList->count()+1).c_str(), 0));
    destinationAmountLabel->setText(QApplication::translate("MultisigDialog", "Amount: ", 0));

    destinationLayout->addWidget(destinationAmount);

    QPushButton* destinationDeleteButton = new QPushButton(destinationFrame);
    destinationDeleteButton->setObjectName(QStringLiteral("destinationDeleteButton"));
    QIcon icon;
    icon.addFile(QStringLiteral(":/icons/remove"), QSize(), QIcon::Normal, QIcon::Off);
    destinationDeleteButton->setIcon(icon);
    destinationDeleteButton->setAutoDefault(false);
    connect(destinationDeleteButton, SIGNAL(clicked()), this, SLOT(deleteFrame()));
    destinationLayout->addWidget(destinationDeleteButton);

    frameLayout->addLayout(destinationLayout);

    ui->destinationsList->addWidget(destinationFrame);
}

void MultisigDialog::on_addPrivKeyButton_clicked()
{
    if(isFirstPrivKey){//on first click the scroll area must show
        isFirstPrivKey = false;
        ui->keyScrollArea->show();
    }

    if(ui->keyList->count() > 14){
        ui->signButtonStatus->setStyleSheet("QTextEdit{ color: red }");
        ui->signButtonStatus->setText(tr("Maximum (15)"));
        return;
    }

    QFrame* keyFrame = new QFrame(ui->keyScrollAreaContents);

    keyFrame->setObjectName(QStringLiteral("keyFrame"));
    keyFrame->setFrameShape(QFrame::StyledPanel);
    keyFrame->setFrameShadow(QFrame::Raised);

    QHBoxLayout* keyLayout = new QHBoxLayout(keyFrame);
    keyLayout->setObjectName(QStringLiteral("keyLayout"));

    QLabel* keyLabel = new QLabel(keyFrame);
    keyLabel->setObjectName(QStringLiteral("keyLabel"));
    keyLabel->setText(QApplication::translate("MultisigDialog", strprintf("Key %i: ", (ui->keyList->count()+1)).c_str(), 0));
    keyLayout->addWidget(keyLabel);

    QLineEdit* key = new QLineEdit(keyFrame);
    key->setObjectName(QStringLiteral("key"));
    key->setEchoMode(QLineEdit::Password);
    keyLayout->addWidget(key);

    QPushButton* keyDeleteButton = new QPushButton(keyFrame);
    keyDeleteButton->setObjectName(QStringLiteral("keyDeleteButton"));
    QIcon icon;
    icon.addFile(QStringLiteral(":/icons/remove"), QSize(), QIcon::Normal, QIcon::Off);
    keyDeleteButton->setIcon(icon);
    keyDeleteButton->setAutoDefault(false);
    connect(keyDeleteButton, SIGNAL(clicked()), this, SLOT(deleteFrame()));
    keyLayout->addWidget(keyDeleteButton);

    ui->keyList->addWidget(keyFrame);
}

