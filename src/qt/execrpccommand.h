#ifndef EXECRPCCOMMAND_H
#define EXECRPCCOMMAND_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

/**
 * @brief The ExecRPCCommand class Execution of RPC command line
 */
class ExecRPCCommand : public QObject
{
public:
    /**
     * @brief ExecRPCCommand Constructor
     * @param command Command to be executed
     * @param mandatory Mandatory parameters
     * @param optional Optional parameters
     * @param translations Translations of the fields
     * @param parent Parent object
     */
    explicit ExecRPCCommand(const QString& command, const QStringList& mandatory, const QStringList& optional, const QMap<QString, QString>& translations, QObject *parent = 0);

    /**
     * @brief exec Execute the RPC command
     * @param params Map of the parameter name and the parameter value
     * @param result Returned data from the PRC call execution
     * @param resultJson Raw JSON string from the PRC call execution
     * @param errorMessage Error message from the execution
     * @return Result of the execution
     */
    bool exec(const QMap<QString, QString>& params, QVariant& result, QString& resultJson, QString& errorMessage);

    /**
     * @brief appendParam Append paramether to the list
     * @param params List of paramethers
     * @param paramName Paramether name
     * @param paramValue Paramether valuse
     */
    static void appendParam(QMap<QString, QString>& params, const QString& paramName, const QString& paramValue);
	
private:
    QString m_command;
    QStringList m_mandatoryParams;
    QStringList m_optionalParams;
    QMap<QString, QString> m_translations;
};

#endif // EXECRPCCOMMAND_H
