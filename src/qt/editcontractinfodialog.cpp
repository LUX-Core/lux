#include "editcontractinfodialog.h"
#include "ui_editcontractinfodialog.h"

#include "contracttablemodel.h"
#include "contractabi.h"
//#include "styleSheet.h"

#include <QDataWidgetMapper>
#include <QMessageBox>
#include <QRegularExpressionValidator>
#include <QPushButton>

EditContractInfoDialog::EditContractInfoDialog(Mode _mode, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EditContractInfoDialog),
    mapper(0),
    mode(_mode),
    model(0),
    m_contractABI(0)
{
    m_contractABI = new ContractABI();

    ui->setupUi(this);

//    SetObjectStyleSheet(ui->buttonBox->button(QDialogButtonBox::Cancel), StyleSheetNames::ButtonWhite);
//    SetObjectStyleSheet(ui->buttonBox->button(QDialogButtonBox::Ok), StyleSheetNames::ButtonBlue);

    switch(mode)
    {
    case NewContractInfo:
        setWindowTitle(tr("New contract info"));
        break;
    case EditContractInfo:
        setWindowTitle(tr("Edit contract info"));
        break;
    }

    mapper = new QDataWidgetMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);

    connect(ui->ABIEdit, SIGNAL(textChanged()), SLOT(on_newContractABI()));

    // Set contract address validator
    QRegularExpression regEx;
    regEx.setPattern(paternAddress);
    QRegularExpressionValidator *addressValidator = new QRegularExpressionValidator(ui->addressEdit);
    addressValidator->setRegularExpression(regEx);
    ui->addressEdit->setCheckValidator(addressValidator);
    ui->addressEdit->setEmptyIsValid(false);
}

EditContractInfoDialog::~EditContractInfoDialog()
{
    delete m_contractABI;
    delete ui;
}

bool EditContractInfoDialog::isValidContractAddress()
{
    ui->addressEdit->checkValidity();
    return ui->addressEdit->isValid();
}

bool EditContractInfoDialog::isValidInterfaceABI()
{
    ui->ABIEdit->checkValidity();
    return ui->ABIEdit->isValid();
}

bool EditContractInfoDialog::isDataValid()
{
    bool dataValid = true;

    if(!isValidContractAddress())
        dataValid = false;
    if(!isValidInterfaceABI())
        dataValid = false;

    return dataValid;
}

void EditContractInfoDialog::setModel(ContractTableModel *_model)
{
    this->model = _model;
    if(!_model)
        return;

    mapper->setModel(_model);
    mapper->addMapping(ui->labelEdit, ContractTableModel::Label);
    mapper->addMapping(ui->addressEdit, ContractTableModel::Address);
    mapper->addMapping(ui->ABIEdit, ContractTableModel::ABI, "plainText");
}

void EditContractInfoDialog::loadRow(int row)
{
    mapper->setCurrentIndex(row);
}

bool EditContractInfoDialog::saveCurrentRow()
{
    if(!model)
        return false;

    model->resetEditStatus();
    switch(mode)
    {
    case NewContractInfo:
        address = model->addRow(
                ui->labelEdit->text(),
                ui->addressEdit->text(),
                ui->ABIEdit->toPlainText());
        if(!address.isEmpty())
            this->ABI = ui->ABIEdit->toPlainText();
        break;
    case EditContractInfo:
        if(mapper->submit())
        {
            this->address = ui->addressEdit->text();
            this->ABI = ui->ABIEdit->toPlainText();
        }
        break;
    }
    bool editError = model->getEditStatus() >= ContractTableModel::DUPLICATE_ADDRESS;
    return !address.isEmpty() && !editError;
}

void EditContractInfoDialog::accept()
{
    if(isDataValid())
    {
        if(!model)
            return;

        if(!saveCurrentRow())
        {
            switch(model->getEditStatus())
            {
            case ContractTableModel::OK:
                // Failed with unknown reason. Just reject.
                break;
            case ContractTableModel::NO_CHANGES:
                // No changes were made during edit operation. Just reject.
                break;
            case ContractTableModel::DUPLICATE_ADDRESS:
                QMessageBox::warning(this, windowTitle(),
                                     tr("The entered address \"%1\" is already in the contract book.").arg(ui->addressEdit->text()),
                                     QMessageBox::Ok, QMessageBox::Ok);
                break;

            }
            return;
        }
        QDialog::accept();
    }
}

void EditContractInfoDialog::on_newContractABI()
{
    std::string json_data = ui->ABIEdit->toPlainText().toStdString();
    if(!m_contractABI->loads(json_data))
    {
        ui->ABIEdit->setIsValidManually(false);
    }
    else
    {
        ui->ABIEdit->setIsValidManually(true);
    }
    m_contractABI->clean();
}

QString EditContractInfoDialog::getAddress() const
{
    return address;
}

void EditContractInfoDialog::setAddress(const QString &_address)
{
    this->address = _address;
    ui->addressEdit->setText(_address);
}

QString EditContractInfoDialog::getABI() const
{
    return ABI;
}

void EditContractInfoDialog::setABI(const QString &_ABI)
{
    this->ABI = _ABI;
    ui->ABIEdit->setText(_ABI);
}
