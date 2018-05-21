#include "token.h"
#include "execrpccommand.h"
#include "contractabi.h"
#include "main.h"
#include "utilmoneystr.h"
#include "base58.h"
#include "utilstrencodings.h"
#include "eventlog.h"
#include "libethcore/ABI.h"

namespace Token_NS
{
    static const std::string TOKEN_ABI = "[{\"constant\":true,\"inputs\":[],\"name\":\"name\",\"outputs\":[{\"name\":\"\",\"type\":\"string\"}],\"payable\":false,\"stateMutability\":\"view\",\"type\":\"function\"},{\"constant\":false,\"inputs\":[{\"name\":\"_spender\",\"type\":\"address\"},{\"name\":\"_value\",\"type\":\"uint256\"}],\"name\":\"approve\",\"outputs\":[{\"name\":\"success\",\"type\":\"bool\"}],\"payable\":false,\"stateMutability\":\"nonpayable\",\"type\":\"function\"},{\"constant\":true,\"inputs\":[],\"name\":\"totalSupply\",\"outputs\":[{\"name\":\"\",\"type\":\"uint256\"}],\"payable\":false,\"stateMutability\":\"view\",\"type\":\"function\"},{\"constant\":false,\"inputs\":[{\"name\":\"_from\",\"type\":\"address\"},{\"name\":\"_to\",\"type\":\"address\"},{\"name\":\"_value\",\"type\":\"uint256\"}],\"name\":\"transferFrom\",\"outputs\":[{\"name\":\"success\",\"type\":\"bool\"}],\"payable\":false,\"stateMutability\":\"nonpayable\",\"type\":\"function\"},{\"constant\":true,\"inputs\":[],\"name\":\"decimals\",\"outputs\":[{\"name\":\"\",\"type\":\"uint8\"}],\"payable\":false,\"stateMutability\":\"view\",\"type\":\"function\"},{\"constant\":false,\"inputs\":[{\"name\":\"_value\",\"type\":\"uint256\"}],\"name\":\"burn\",\"outputs\":[{\"name\":\"success\",\"type\":\"bool\"}],\"payable\":false,\"stateMutability\":\"nonpayable\",\"type\":\"function\"},{\"constant\":true,\"inputs\":[{\"name\":\"\",\"type\":\"address\"}],\"name\":\"balanceOf\",\"outputs\":[{\"name\":\"\",\"type\":\"uint256\"}],\"payable\":false,\"stateMutability\":\"view\",\"type\":\"function\"},{\"constant\":false,\"inputs\":[{\"name\":\"_from\",\"type\":\"address\"},{\"name\":\"_value\",\"type\":\"uint256\"}],\"name\":\"burnFrom\",\"outputs\":[{\"name\":\"success\",\"type\":\"bool\"}],\"payable\":false,\"stateMutability\":\"nonpayable\",\"type\":\"function\"},{\"constant\":true,\"inputs\":[],\"name\":\"symbol\",\"outputs\":[{\"name\":\"\",\"type\":\"string\"}],\"payable\":false,\"stateMutability\":\"view\",\"type\":\"function\"},{\"constant\":false,\"inputs\":[{\"name\":\"_to\",\"type\":\"address\"},{\"name\":\"_value\",\"type\":\"uint256\"}],\"name\":\"transfer\",\"outputs\":[],\"payable\":false,\"stateMutability\":\"nonpayable\",\"type\":\"function\"},{\"constant\":false,\"inputs\":[{\"name\":\"_spender\",\"type\":\"address\"},{\"name\":\"_value\",\"type\":\"uint256\"},{\"name\":\"_extraData\",\"type\":\"bytes\"}],\"name\":\"approveAndCall\",\"outputs\":[{\"name\":\"success\",\"type\":\"bool\"}],\"payable\":false,\"stateMutability\":\"nonpayable\",\"type\":\"function\"},{\"constant\":true,\"inputs\":[{\"name\":\"\",\"type\":\"address\"},{\"name\":\"\",\"type\":\"address\"}],\"name\":\"allowance\",\"outputs\":[{\"name\":\"\",\"type\":\"uint256\"}],\"payable\":false,\"stateMutability\":\"view\",\"type\":\"function\"},{\"inputs\":[{\"name\":\"initialSupply\",\"type\":\"uint256\"},{\"name\":\"tokenName\",\"type\":\"string\"},{\"name\":\"decimalUnits\",\"type\":\"uint8\"},{\"name\":\"tokenSymbol\",\"type\":\"string\"}],\"payable\":false,\"stateMutability\":\"nonpayable\",\"type\":\"constructor\"},{\"anonymous\":false,\"inputs\":[{\"indexed\":true,\"name\":\"from\",\"type\":\"address\"},{\"indexed\":true,\"name\":\"to\",\"type\":\"address\"},{\"indexed\":false,\"name\":\"value\",\"type\":\"uint256\"}],\"name\":\"Transfer\",\"type\":\"event\"},{\"anonymous\":false,\"inputs\":[{\"indexed\":true,\"name\":\"from\",\"type\":\"address\"},{\"indexed\":false,\"name\":\"value\",\"type\":\"uint256\"}],\"name\":\"Burn\",\"type\":\"event\"}]";

    static const QString PRC_CALL = "callcontract";
    static const QString PRC_SENDTO = "sendtocontract";
    static const QString PARAM_ADDRESS = "address";
    static const QString PARAM_DATAHEX = "datahex";
    static const QString PARAM_AMOUNT = "amount";
    static const QString PARAM_GASLIMIT = "gaslimit";
    static const QString PARAM_GASPRICE = "gasprice";
    static const QString PARAM_SENDER = "sender";
    static const QString PARAM_BROADCAST = "broadcast";
    static const QString PARAM_CHANGE_TO_SENDER = "changeToSender";
}
using namespace Token_NS;

struct TokenData
{
    QMap<QString, QString> lstParams;
    std::string address;
    ExecRPCCommand* call;
    ExecRPCCommand* send;
    EventLog* eventLog;
    ContractABI* ABI;
    int funcName;
    int funcApprove;
    int funcTotalSupply;
    int funcTransferFrom;
    int funcDecimals;
    int funcBurn;
    int funcBalanceOf;
    int funcBurnFrom;
    int funcSymbol;
    int funcTransfer;
    int funcApproveAndCall;
    int funcAllowance;
    int evtTransfer;
    int evtBurn;

    std::string txid;

    TokenData():
            call(0),
            send(0),
            ABI(0),
            funcName(-1),
            funcApprove(-1),
            funcTotalSupply(-1),
            funcTransferFrom(-1),
            funcDecimals(-1),
            funcBurn(-1),
            funcBalanceOf(-1),
            funcBurnFrom(-1),
            funcSymbol(-1),
            funcTransfer(-1),
            funcApproveAndCall(-1),
            funcAllowance(-1),
            evtTransfer(-1),
            evtBurn(-1)
    {}
};

bool ToHash160(const std::string& strLuxAddress, std::string& strHash160)
{   
    CTxDestination luxAddress = DecodeDestination(strLuxAddress);
    if(IsValidDestination(luxAddress)){     
        CKeyID *keyid = boost::get<CKeyID>(&luxAddress);
        strHash160 = HexStr(valtype(keyid->begin(),keyid->end()));
    }else{
        return false;
    }
    return true;
}

bool ToLuxAddress(const std::string& strHash160, std::string& strLuxAddress)
{
    uint160 key(ParseHex(strHash160.c_str()));
    CKeyID keyid(key);
    
    if(IsValidDestination(CTxDestination(keyid))){
        strLuxAddress = EncodeDestination(CTxDestination(keyid));
        return true;
    }
    return false;
}

Token::Token():
        d(0)
{
    d = new TokenData();
    clear();

    QStringList lstMandatory;
    lstMandatory.append(PARAM_ADDRESS);
    lstMandatory.append(PARAM_DATAHEX);
    QStringList lstOptional;
    lstOptional.append(PARAM_SENDER);
    d->call = new ExecRPCCommand(PRC_CALL, lstMandatory, lstOptional, QMap<QString, QString>());

    lstMandatory.clear();
    lstMandatory.append(PARAM_ADDRESS);
    lstMandatory.append(PARAM_DATAHEX);
    lstOptional.clear();
    lstOptional.append(PARAM_AMOUNT);
    lstOptional.append(PARAM_GASLIMIT);
    lstOptional.append(PARAM_GASPRICE);
    lstOptional.append(PARAM_SENDER);
    lstOptional.append(PARAM_BROADCAST);
    lstOptional.append(PARAM_CHANGE_TO_SENDER);
    d->send = new ExecRPCCommand(PRC_SENDTO, lstMandatory, lstOptional, QMap<QString, QString>());
    d->eventLog = new EventLog();

    // Compute functions indexes
    d->ABI = new ContractABI();
    if(d->ABI->loads(TOKEN_ABI))
    {
        for(size_t i = 0; i < d->ABI->functions.size(); i++)
        {
            FunctionABI func = d->ABI->functions[i];
            if(func.name == "name")
            {
                d->funcName = i;
            }
            else if(func.name == "approve")
            {
                d->funcApprove = i;
            }
            else if(func.name == "totalSupply")
            {
                d->funcTotalSupply = i;
            }
            else if(func.name == "transferFrom")
            {
                d->funcTransferFrom = i;
            }
            else if(func.name == "decimals")
            {
                d->funcDecimals = i;
            }
            else if(func.name == "burn")
            {
                d->funcBurn = i;
            }
            else if(func.name == "balanceOf")
            {
                d->funcBalanceOf = i;
            }
            else if(func.name == "burnFrom")
            {
                d->funcBurnFrom = i;
            }
            else if(func.name == "symbol")
            {
                d->funcSymbol = i;
            }
            else if(func.name == "transfer")
            {
                d->funcTransfer = i;
            }
            else if(func.name == "approveAndCall")
            {
                d->funcApproveAndCall = i;
            }
            else if(func.name == "allowance")
            {
                d->funcAllowance = i;
            }
            else if(func.name == "Transfer")
            {
                d->evtTransfer = i;
            }
            else if(func.name == "Burn")
            {
                d->evtBurn = i;
            }
        }
    }
}

Token::~Token()
{
    if(d->call)
        delete d->call;
    d->call = 0;

    if(d->send)
        delete d->send;
    d->send = 0;

    if(d->ABI)
        delete d->ABI;
    d->ABI = 0;

    if(d)
        delete d;
    d = 0;
}

void Token::setAddress(const std::string &address)
{
    d->lstParams[PARAM_ADDRESS] = QString::fromStdString(address);
}

void Token::setDataHex(const std::string &datahex)
{
    d->lstParams[PARAM_DATAHEX] = QString::fromStdString(datahex);
}

void Token::setAmount(const std::string &amount)
{
    d->lstParams[PARAM_AMOUNT] = QString::fromStdString(amount);
}

void Token::setGasLimit(const std::string &gaslimit)
{
    d->lstParams[PARAM_GASLIMIT] = QString::fromStdString(gaslimit);
}

void Token::setGasPrice(const std::string &gasPrice)
{
    d->lstParams[PARAM_GASPRICE] = QString::fromStdString(gasPrice);
}

void Token::setSender(const std::string &sender)
{
    d->lstParams[PARAM_SENDER] = QString::fromStdString(sender);
}

void Token::clear()
{
    d->lstParams.clear();

    setAmount("0");
    setGasPrice(FormatMoney(DEFAULT_GAS_PRICE));
    setGasLimit(std::to_string(DEFAULT_GAS_LIMIT_OP_SEND));

    d->lstParams[PARAM_BROADCAST] = "true";
    d->lstParams[PARAM_CHANGE_TO_SENDER] = "true";
}

std::string Token::getTxId()
{
    return d->txid;
}

bool Token::name(std::string &result, bool sendTo)
{
    std::vector<std::string> input;
    std::vector<std::string> output;
    if(!exec(input, d->funcName, output, sendTo))
        return false;

    if(!sendTo)
    {
        if(output.size() == 0)
            return false;
        else
            result = output[0];
    }

    return true;
}

bool Token::approve(const std::string &_spender, const std::string &_value, bool &success, bool sendTo)
{
    std::vector<std::string> input;
    input.push_back(_spender);
    input.push_back(_value);
    std::vector<std::string> output;

    if(!exec(input, d->funcApprove, output, sendTo))
        return false;

    if(!sendTo)
    {
        if(output.size() == 0)
            return false;
        else
            success = output[0] == "true";
    }

    return true;
}

bool Token::totalSupply(std::string &result, bool sendTo)
{
    std::vector<std::string> input;
    std::vector<std::string> output;
    if(!exec(input, d->funcTotalSupply, output, sendTo))
        return false;

    if(!sendTo)
    {
        if(output.size() == 0)
            return false;
        else
            result = output[0];
    }

    return true;
}

bool Token::transferFrom(const std::string &_from, const std::string &_to, const std::string &_value, bool &success, bool sendTo)
{
    std::vector<std::string> input;
    input.push_back(_from);
    input.push_back(_to);
    input.push_back(_value);
    std::vector<std::string> output;

    if(!exec(input, d->funcTransferFrom, output, sendTo))
        return false;

    if(!sendTo)
    {
        if(output.size() == 0)
            return false;
        else
            success = output[0] == "true";
    }

    return true;
}

bool Token::decimals(std::string &result, bool sendTo)
{
    std::vector<std::string> input;
    std::vector<std::string> output;
    if(!exec(input, d->funcDecimals, output, sendTo))
        return false;

    if(!sendTo)
    {
        if(output.size() == 0)
            return false;
        else
            result = output[0];
    }

    return true;
}

bool Token::burn(const std::string &_value, bool &success, bool sendTo)
{
    std::vector<std::string> input;
    input.push_back(_value);
    std::vector<std::string> output;

    if(!exec(input, d->funcBurn, output, sendTo))
        return false;

    if(!sendTo)
    {
        if(output.size() == 0)
            return false;
        else
            success = output[0] == "true";
    }

    return true;
}

bool Token::balanceOf(std::string &result, bool sendTo)
{
    std::string spender = d->lstParams[PARAM_SENDER].toStdString();
    if(!ToHash160(spender, spender))
    {
        return false;
    }

    return balanceOf(spender, result, sendTo);
}

bool Token::balanceOf(const std::string &spender, std::string &result, bool sendTo)
{
    std::vector<std::string> input;
    input.push_back(spender);
    std::vector<std::string> output;

    if(!exec(input, d->funcBalanceOf, output, sendTo))
        return false;

    if(!sendTo)
    {
        if(output.size() == 0)
            return false;
        else
            result = output[0];
    }

    return true;
}

bool Token::burnFrom(const std::string &_from, const std::string &_value, bool &success, bool sendTo)
{
    std::vector<std::string> input;
    input.push_back(_from);
    input.push_back(_value);
    std::vector<std::string> output;

    if(!exec(input, d->funcBurnFrom, output, sendTo))
        return false;

    if(!sendTo)
    {
        if(output.size() == 0)
            return false;
        else
            success = output[0] == "true";
    }

    return true;
}

bool Token::symbol(std::string &result, bool sendTo)
{
    std::vector<std::string> input;
    std::vector<std::string> output;
    if(!exec(input, d->funcSymbol, output, sendTo))
        return false;

    if(!sendTo)
    {
        if(output.size() == 0)
            return false;
        else
            result = output[0];
    }

    return true;
}

bool Token::transfer(const std::string &_to, const std::string &_value, bool sendTo)
{
    std::string to = _to;
    if(!ToHash160(to, to))
    {
        return false;
    }

    std::vector<std::string> input;
    input.push_back(to);
    input.push_back(_value);
    std::vector<std::string> output;

    return exec(input, d->funcTransfer, output, sendTo);
}

bool Token::approveAndCall(const std::string &_spender, const std::string &_value, const std::string &_extraData, bool &success, bool sendTo)
{
    std::vector<std::string> input;
    input.push_back(_spender);
    input.push_back(_value);
    input.push_back(_extraData);
    std::vector<std::string> output;

    if(!exec(input, d->funcApproveAndCall, output, sendTo))
        return false;

    if(!sendTo)
    {
        if(output.size() == 0)
            return false;
        else
            success = output[0] == "true";
    }

    return true;
}

bool Token::allowance(const std::string &_from, const std::string &_to, std::string &result, bool sendTo)
{
    std::vector<std::string> input;
    input.push_back(_from);
    input.push_back(_to);
    std::vector<std::string> output;

    if(!exec(input, d->funcAllowance, output, sendTo))
        return false;

    if(!sendTo)
    {
        if(output.size() == 0)
            return false;
        else
            result = output[0];
    }

    return true;
}

bool Token::transferEvents(std::vector<TokenEvent> &tokenEvents, int64_t fromBlock, int64_t toBlock)
{
    return execEvents(fromBlock, toBlock, d->evtTransfer, tokenEvents);
}

bool Token::burnEvents(std::vector<TokenEvent> &tokenEvents, int64_t fromBlock, int64_t toBlock)
{
    return execEvents(fromBlock, toBlock, d->evtBurn, tokenEvents);
}

bool Token::exec(const std::vector<std::string> &input, int func, std::vector<std::string> &output, bool sendTo)
{
    d->txid = "";
    if(func == -1)
        return false;
    std::string strData;
    FunctionABI function = d->ABI->functions[func];
    std::vector<std::vector<std::string>> values;
    for(size_t i = 0; i < input.size(); i++)
    {
        std::vector<std::string> param;
        param.push_back(input[i]);
        values.push_back(param);
    }
    std::vector<ParameterABI::ErrorType> errors;
    if(!function.abiIn(values, strData, errors))
        return false;
    setDataHex(strData);

    ExecRPCCommand* cmd = sendTo ? d->send : d->call;
    QVariant result;
    QString resultJson;
    QString errorMessage;
    if(!cmd->exec(d->lstParams, result, resultJson, errorMessage))
        return false;

    if(!sendTo)
    {
        QVariantMap variantMap = result.toMap();
        QVariantMap executionResultMap = variantMap.value("executionResult").toMap();
        std::string rawData = executionResultMap.value("output").toString().toStdString();
        std::vector<std::vector<std::string>> values;
        std::vector<ParameterABI::ErrorType> errors;
        if(!function.abiOut(rawData, values, errors))
            return false;
        for(size_t i = 0; i < values.size(); i++)
        {
            std::vector<std::string> param = values[i];
            output.push_back(param.size() ? param[0] : "");
        }
    }
    else
    {
        QVariantMap variantMap = result.toMap();
        d->txid = variantMap.value("txid").toString().toStdString();
    }

    return true;
}

bool Token::execEvents(int64_t fromBlock, int64_t toBlock, int func, std::vector<TokenEvent> &tokenEvents)
{
    // Check parameters
    if(func == -1 || fromBlock < 0)
        return false;

    //  Get function
    FunctionABI function = d->ABI->functions[func];

    // Search for events
    QVariant result;
    std::string eventName = function.selector();
    std::string contractAddress = d->lstParams[PARAM_ADDRESS].toStdString();
    std::string senderAddress = d->lstParams[PARAM_SENDER].toStdString();
    ToHash160(senderAddress, senderAddress);
    senderAddress  = "000000000000000000000000" + senderAddress;
    if(!(d->eventLog->searchTokenTx(fromBlock, toBlock, contractAddress, senderAddress, result)))
        return false;

    // Parse the result events
    QList<QVariant> list = result.toList();
    for(int i = 0; i < list.size(); i++)
    {
        QVariantMap variantMap = list[i].toMap();

        QList<QVariant> listLog = variantMap.value("log").toList();
        for(int i = 0; i < listLog.size(); i++)
        {
            // Skip the not needed events
            QVariantMap variantLog = listLog[0].toMap();
            QList<QVariant> topicsList = variantLog.value("topics").toList();
            if(topicsList.count() < 3) continue;
            if(topicsList[0].toString().toStdString() != eventName) continue;

            // Create new event
            TokenEvent tokenEvent;
            tokenEvent.address = variantMap.value("contractAddress").toString().toStdString();
            tokenEvent.sender = topicsList[1].toString().toStdString().substr(24);
            ToLuxAddress(tokenEvent.sender, tokenEvent.sender);
            tokenEvent.receiver = topicsList[2].toString().toStdString().substr(24);
            ToLuxAddress(tokenEvent.receiver, tokenEvent.receiver);
            tokenEvent.blockHash = uint256S(variantMap.value("blockHash").toString().toStdString());
            tokenEvent.blockNumber = variantMap.value("blockNumber").toLongLong();
            tokenEvent.transactionHash = uint256S(variantMap.value("transactionHash").toString().toStdString());

            // Parse data
            std::string data = variantLog.value("data").toString().toStdString();
            dev::bytes rawData = dev::fromHex(data);
            dev::bytesConstRef o(&rawData);
            dev::u256 outData = dev::eth::ABIDeserialiser<dev::u256>::deserialise(o);
            tokenEvent.value = u256Touint(outData);

            tokenEvents.push_back(tokenEvent);
        }
    }

    return true;
}
