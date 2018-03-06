// Copyright (c) 2016-2017 The Qtum Core developers
// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ADDRESSFIELD_H
#define ADDRESSFIELD_H

#include <QComboBox>
#include <QStringList>
#include <QStringListModel>


/** Drop down list of addresses
  */
class AddressField: public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(AddressType addressType READ addressType WRITE setAddressType NOTIFY addressTypeChanged)

public:
    /**
     * @brief The AddressType enum Type of addresses that will be displayed
     */
    enum AddressType { UTXO };

    Q_ENUM(AddressType)

    /**
     * @brief setAddressType Set the address type
     * @param addressType Address type
     */
    void setAddressType(AddressType addressType)
    {
        m_addressType = addressType;
        Q_EMIT addressTypeChanged(m_addressType);
    }

    /**
     * @brief addressType Get the address type
     * @return Address type
     */
    AddressType addressType() const
    { 
        return m_addressType; 
    }

    /**
     * @brief AddressField Constructor
     * @param parent Parent widget
     */
    explicit AddressField(QWidget *parent = 0);

    /**
     * @brief currentText Get the current text
     * @return Current text
     */
    virtual QString currentText() const;

    bool isValidAddress();
    void setComboBoxEditable(bool editable);

    void setAddressTableModel(QAbstractItemModel *addressTableModel);

    void setAddressColumn(int addressColumn);

    void setTypeRole(int typeRole);

    void setReceive(const QString &receive);

Q_SIGNALS:
    /**
     * @brief addressTypeChanged Signal that the address type is changed
     */
    void addressTypeChanged(AddressType);

public Q_SLOTS:
    /**
     * @brief on_refresh Refresh the list of addresses
     */
    void on_refresh();

    /**
     * @brief on_addressTypeChanged Change the address type
     */
    void on_addressTypeChanged();

    /**
     * @brief on_editingFinished Completer finish text update
     */
    void on_editingFinished();

private:
    void appendAddress(const QString& strAddress);

private:
    QStringList m_stringList;
    QStringListModel m_stringModel;
    AddressType m_addressType;
    QAbstractItemModel* m_addressTableModel;
    int m_addressColumn;
    int m_typeRole;
    QString m_receive;
};

#endif // ADDRESSFIELD_H
