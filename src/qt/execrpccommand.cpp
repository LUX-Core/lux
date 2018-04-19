#include "execrpccommand.h"
#include "rpcconsole.h"
#include <QJsonDocument>
#include "univalue/univalue.h"

ExecRPCCommand::ExecRPCCommand(const QString &command, const QStringList &mandatory, const QStringList &optional, const QMap<QString, QString>& translations, QObject *parent)
    : QObject(parent)
{
    m_command = command;
    m_mandatoryParams = mandatory;
    m_optionalParams = optional;
    m_translations = translations;
}

bool ExecRPCCommand::exec(const QMap<QString, QString> &params, QVariant &result, QString &resultJson, QString &errorMessage) {
    QStringList commandLine;
    commandLine.append(m_command);

    // Check the list of mandatory parameters
    QStringList mandatoryNotPresent;
    for (int i = 0; i < m_mandatoryParams.count(); i++) {
        QString key = m_mandatoryParams[i];
        if (!params.contains(key)) {
            mandatoryNotPresent.append(m_translations[key]);
        } else {
            commandLine.append(params[key]);
        }
    }
    if (mandatoryNotPresent.count() > 0) {
        errorMessage = tr("Mandatory fields are not present:\n%1").arg(mandatoryNotPresent.join(", "));
        return false;
    }

    // Check the list of optional parameters
    bool haveOptional = false;
    int optionalParamsAt = commandLine.size();
    QStringList optionalNotPresent;
    for (int i = m_optionalParams.count() - 1; i > -1; i--) {
        QString key = m_optionalParams[i];
        if (params.contains(key)) {
            if (!haveOptional) haveOptional = true;
            commandLine.insert(optionalParamsAt, params[key]);
        } else {
            if (haveOptional) {
                optionalNotPresent.prepend(m_translations[key]);
            }
        }
    }
    if (optionalNotPresent.count() > 0) {
        errorMessage = tr("Optional fields are not present:\n%1").arg(optionalNotPresent.join(", "));
        return false;
    }

    // Execute the RPC command
    try {
        std::string strResult;
        std::string strCommand = commandLine.join(' ').toStdString();
        if (RPCConsole::RPCExecuteCommandLine(strResult, strCommand)) {
            resultJson = strResult.c_str();
            QJsonDocument doc = QJsonDocument::fromJson(strResult.c_str());
            result = doc.toVariant();
            return true;
        } else {
            errorMessage = tr("Parse error: unbalanced ' or \"");
        }
    } catch (UniValue& objError) {
        try { // Nice formatting for standard-format error

            int code = find_value(objError, "code").get_int();
            std::string message = find_value(objError, "message").get_str();
            errorMessage = QString::fromStdString(message) + " (code " + QString::number(code) + ")";
        } catch (const std::runtime_error&) { // raised when converting to invalid type, i.e. missing code or message
            // Show raw JSON object
            errorMessage = QString::fromStdString(objError.write());
        }
    } catch (const std::exception& e) {

        errorMessage = QString("Error: ") + QString::fromStdString(e.what());
    }

    return false;
}

void ExecRPCCommand::appendParam(QMap<QString, QString> &params, const QString &paramName, const QString &paramValue) {
    QString _paramValue = paramValue.trimmed();
    if(!(_paramValue.isNull() || _paramValue.isEmpty())) params[paramName] = _paramValue;
}


