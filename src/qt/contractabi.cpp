#include "contractabi.h"
#include "univalue.h"
#include "libethcore/ABI.h"
#include <math.h>

namespace ContractABI_NS
{
// Defining json preprocessor functions in order to avoid repetitive code with slight difference
#define ReadJsonString(json, param, result) if(json.exists(#param) && json[#param].isStr())\
    result.param = json[#param].get_str();
#define ReadJsonBool(json, param, result) if(json.exists(#param) && json[#param].isBool())\
    result.param = json[#param].get_bool();
#define ReadJsonArray(json, param, result) if(json.exists(#param) && json[#param].isArray())\
    result = json[#param].get_array();

// String parsing functions
inline bool startsWithString(const std::string& str, const std::string& s, size_t& pos)
{
    if(pos >= str.length()) return false;

    size_t length = s.length();
    bool ret = (str.substr(pos, length) == s);
    if(ret) pos += length;
    return ret;
}

inline std::string startsWithNumber(const std::string& str, size_t& pos)
{
    if(pos >= str.length()) return "";

    std::stringstream ss;
    for(size_t i = pos; i < str.size(); i++)
    {
        char c = str[i];
        if(c >= '0' && c <= '9')
            ss << c;
        else
            break;
    }

    std::string s = ss.str();
    pos += s.length();
    return s;
}

// define constransts
const static int HEX_INSTRUCTION_SIZE = 64;
}
using namespace ContractABI_NS;

ContractABI::ContractABI()
{}

bool ContractABI::loads(const std::string &json_data)
{
    clean();

    UniValue json_contract;
    bool ret = json_contract.read(json_data);
    if(ret && json_contract.isArray())
    {
        // Read all functions from the contract
        size_t size = json_contract.size();
        for(size_t i = 0; i < size; i++)
        {
            const UniValue& json_function = json_contract[i];
            FunctionABI function;
            ReadJsonString(json_function, name, function);
            ReadJsonString(json_function, type, function);
            ReadJsonBool(json_function, payable, function);
            ReadJsonBool(json_function, constant, function);
            ReadJsonBool(json_function, anonymous, function);

            UniValue json_inputs;
            ReadJsonArray(json_function, inputs, json_inputs);
            for(size_t j = 0; j < json_inputs.size(); j++)
            {
                const UniValue& json_param = json_inputs[j];
                ParameterABI param;
                ReadJsonString(json_param, name, param);
                ReadJsonString(json_param, type, param);
                function.inputs.push_back(param);
            }

            UniValue json_outputs;
            ReadJsonArray(json_function, outputs, json_outputs);
            for(size_t j = 0; j < json_outputs.size(); j++)
            {
                const UniValue& json_param = json_outputs[j];
                ParameterABI param;
                ReadJsonString(json_param, name, param);
                ReadJsonString(json_param, type, param);
                ReadJsonBool(json_param, indexed, param);
                function.outputs.push_back(param);
            }

            functions.push_back(function);
        }
    }

    FunctionABI function;
    function.type = "default";
    function.payable = true;
    functions.push_back(function);

    return ret;
}

void ContractABI::clean()
{
    functions.clear();
}

FunctionABI::FunctionABI(const std::string &_name,
                         const std::string &_type,
                         const std::vector<ParameterABI> &_inputs,
                         const std::vector<ParameterABI> &_outputs,
                         bool _payable, bool _constant, bool _anonymous):
    name(_name),
    type(_type),
    inputs(_inputs),
    outputs(_outputs),
    payable(_payable),
    constant(_constant),
    anonymous(_anonymous)
{}

bool FunctionABI::abiIn(const std::vector<std::vector<std::string>> &values, std::string &data, std::vector<ParameterABI::ErrorType>& errors) const
{
    bool ret = inputs.size() == values.size();
    std::string params;
    std::map<int, std::string> mapDynamic;
    for(size_t i = 0; i < inputs.size(); i++)
    {
        ret &= inputs[i].abiIn(values[i], params, mapDynamic);
        errors.push_back(inputs[i].lastError());
    }
    if(ret)
    {
        processDynamicParams(mapDynamic, params);
        data = selector() + params;
    }
    return ret;
}

bool FunctionABI::abiOut(const std::string &data, std::vector<std::vector<std::string>> &values, std::vector<ParameterABI::ErrorType>& errors) const
{
    size_t pos = 0;
    bool ret = true;
    for(size_t i = 0; i < outputs.size(); i++)
    {
        std::vector<std::string> value;
        ret &= outputs[i].abiOut(data, pos, value);
        values.push_back(value);
        errors.push_back(outputs[i].lastError());
    }
    return ret;
}

std::string FunctionABI::selector() const
{
    if(type == "default")
    {
        return defaultSelector();
    }

    if(type == "constructor" || (type == "event" && anonymous))
    {
        return "";
    }

    std::stringstream id;
    id << name;
    id << "(";
    if(inputs.size() > 0)
    {
        id << inputs[0].type;
    }
    for(size_t i = 1; i < inputs.size(); i++)
    {
        id << "," << inputs[i].type;
    }
    id << ")";
    std::string sig = id.str();

    dev::bytes hash;
    if(type == "event")
    {
        hash = dev::sha3(sig).ref().toBytes();
    }
    else
    {
        hash = dev::sha3(sig).ref().cropped(0, 4).toBytes();
    }

    return dev::toHex(hash);
}

std::string FunctionABI::defaultSelector()
{
    return "00";
}

QString FunctionABI::errorMessage(std::vector<ParameterABI::ErrorType> &errors, bool in) const
{
    if(in && errors.size() != inputs.size())
        return "";
    if(!in && errors.size() != outputs.size())
        return "";
    const std::vector<ParameterABI>& params = in ? inputs : outputs;

    QStringList messages;
    messages.append(QObject::tr("ABI parsing error:"));
    for(size_t i = 0; i < errors.size(); i++)
    {
        ParameterABI::ErrorType err = errors[i];
        if(err == ParameterABI::Ok) continue;
        const ParameterABI& param = params[i];
        QString _type = QString::fromStdString(param.type);
        QString _name = QString::fromStdString(param.name);

        switch (err) {
        case ParameterABI::UnsupportedABI:
            messages.append(QObject::tr("Unsupported type %1 %2.").arg(_type, _name));
            break;
        case ParameterABI::EncodingError:
            messages.append(QObject::tr("Error encoding parameter %1 %2.").arg(_type, _name));
            break;
        case ParameterABI::DecodingError:
            messages.append(QObject::tr("Error decoding parameter %1 %2.").arg(_type, _name));
            break;
        default:
            break;
        }
    }
    return messages.join('\n');
}

void FunctionABI::processDynamicParams(const std::map<int, std::string> &mapDynamic, std::string &data) const
{
    for(auto i = mapDynamic.begin(); i != mapDynamic.end(); i++)
    {
        int pos = i->first;
        std::string value = i->second;
        dev::u256 inRef = data.size() / 2;
        dev::bytes rawRef = dev::eth::ABISerialiser<dev::u256>::serialise(inRef);
        std::string strRef = dev::toHex(rawRef);
        data.replace(pos, strRef.size(), strRef);
        data += value;
    }
}

ParameterABI::ParameterABI(const std::string &_name, const std::string &_type, bool _indexed):
    name(_name),
    type(_type),
    indexed(_indexed),
    m_lastError(ParameterABI::Ok)
{}

ParameterABI::~ParameterABI()
{}

bool ParameterABI::abiInBasic(ParameterType::Type abiType, std::string value, std::string &data) const
{
    switch (abiType) {
    case ParameterType::abi_bytes:
    {
        value = dev::asString(dev::fromHex(value));
        dev::string32 inData = dev::eth::toString32(value);
        data += dev::toHex(inData);
    }
        break;
    case ParameterType::abi_bool:
        value = value == "false" ? "0" : "1";
    case ParameterType::abi_int:
    case ParameterType::abi_uint:
    {
        dev::u256 inData(value.c_str());
        dev::bytes rawData = dev::eth::ABISerialiser<dev::u256>::serialise(inData);
        data += dev::toHex(rawData);
    }
        break;
    case ParameterType::abi_address:
    {
        dev::u160 inData = dev::fromBigEndian<dev::u160, dev::bytes>(dev::fromHex(value));
        dev::bytes rawData = dev::eth::ABISerialiser<dev::u160>::serialise(inData);
        data += dev::toHex(rawData);
    }
        break;
    default:
        m_lastError = UnsupportedABI;
        return false;
    }
    return true;
}

bool ParameterABI::abiOutBasic(ParameterType::Type abiType, const std::string &data, size_t &pos, std::string &value) const
{
    switch (abiType) {
    case ParameterType::abi_bytes:
    {
        dev::bytes rawData = dev::fromHex(data.substr(pos, HEX_INSTRUCTION_SIZE));
        dev::bytesConstRef o(&rawData);
        std::string outData = dev::toString(dev::eth::ABIDeserialiser<dev::string32>::deserialise(o));
        value = dev::toHex(outData);
    }
        break;
    case ParameterType::abi_uint:
    {
        dev::bytes rawData = dev::fromHex(data.substr(pos, HEX_INSTRUCTION_SIZE));
        dev::bytesConstRef o(&rawData);
        dev::u256 outData = dev::eth::ABIDeserialiser<dev::u256>::deserialise(o);
        value = outData.str();
    }
        break;
    case ParameterType::abi_int:
    {
        dev::bytes rawData = dev::fromHex(data.substr(pos, HEX_INSTRUCTION_SIZE));
        dev::bytesConstRef o(&rawData);
        dev::s256 outData = dev::u2s(dev::eth::ABIDeserialiser<dev::u256>::deserialise(o));
        value = outData.str();
    }
        break;
    case ParameterType::abi_address:
    {
        dev::bytes rawData = dev::fromHex(data.substr(pos, HEX_INSTRUCTION_SIZE));
        dev::bytesConstRef o(&rawData);
        dev::u160 outData = dev::eth::ABIDeserialiser<dev::u160>::deserialise(o);
        dev::bytes rawAddress(20);
        dev::toBigEndian<dev::u160, dev::bytes>(outData, rawAddress);
        value = dev::toHex(rawAddress);
    }
        break;
    case ParameterType::abi_bool:
    {
        dev::bytes rawData = dev::fromHex(data.substr(pos, HEX_INSTRUCTION_SIZE));
        dev::bytesConstRef o(&rawData);
        dev::u256 outData = dev::eth::ABIDeserialiser<dev::u256>::deserialise(o);
        value = outData == 0 ? "false" : "true";
    }
        break;
    default:
        m_lastError = UnsupportedABI;
        return false;
    }

    pos += HEX_INSTRUCTION_SIZE;

    return true;
}

void ParameterABI::addDynamic(const std::string &paramData, std::string &data, std::map<int, std::string> &mapDynamic) const
{
    int key = data.size();
    data += paramData.substr(0, HEX_INSTRUCTION_SIZE);
    mapDynamic[key] = paramData.substr(HEX_INSTRUCTION_SIZE);
}

bool ParameterABI::abiIn(const std::vector<std::string> &value, std::string &data, std::map<int, std::string>& mapDynamic) const
{
    try
    {
        m_lastError = Ok;
        ParameterType::Type abiType = decodeType().type();
        if(decodeType().isDynamic() && !decodeType().isList())
        {
            // Dynamic basic type (list of bytes or chars)
            std::string _value = value[0];
            switch (abiType) {
            case ParameterType::abi_bytes:
                _value = dev::asString(dev::fromHex(_value));
            case ParameterType::abi_string:
            {
                std::string paramData = dev::toHex(dev::eth::ABISerialiser<std::string>::serialise(_value));
                addDynamic(paramData, data, mapDynamic);
            }
                break;
            default:
                m_lastError = UnsupportedABI;
                return false;
            }
        }
        else if(!decodeType().isDynamic() && !decodeType().isList())
        {
            // Static basic type
            abiInBasic(abiType, value[0], data);
        }
        else if(decodeType().isDynamic() && decodeType().isList())
        {
            // Dynamic list type
            std::string paramData;
            abiInBasic(ParameterType::abi_uint, "32", paramData);
            size_t length = value.size();
            abiInBasic(ParameterType::abi_uint, std::to_string(length), paramData);
            for(size_t i = 0; i < length; i++)
            {
                abiInBasic(abiType, value[i], paramData);
            }
            addDynamic(paramData, data, mapDynamic);
        }
        else if(!decodeType().isDynamic() && decodeType().isList())
        {
            // Static list type
            size_t length = decodeType().length();
            for(size_t i = 0; i < length; i++)
            {
                abiInBasic(abiType, value[i], data);
            }
        }
        else
        {
            // Unknown type
            m_lastError = UnsupportedABI;
            return false;
        }
    }
    catch(...)
    {
        m_lastError = EncodingError;
        return false;
    }

    return true;
}

bool ParameterABI::abiOut(const std::string &data, size_t &pos, std::vector<std::string> &value) const
{
    try
    {
        m_lastError = Ok;
        ParameterType::Type abiType = decodeType().type();
        if(decodeType().isDynamic() && !decodeType().isList())
        {
            // Dynamic basic type
            switch (abiType) {
            case ParameterType::abi_bytes:
            {
                dev::bytes rawData = dev::fromHex(data.substr(pos));
                dev::bytesConstRef o(&rawData);
                std::string outData = dev::eth::ABIDeserialiser<std::string>::deserialise(o);
                value.push_back(dev::toHex(outData));
            }
                break;
            case ParameterType::abi_string:
            {
                dev::bytes rawData = dev::fromHex(data.substr(pos));
                dev::bytesConstRef o(&rawData);
                value.push_back(dev::eth::ABIDeserialiser<std::string>::deserialise(o));
            }
                break;
            default:
                m_lastError = UnsupportedABI;
                return false;
            }

            pos += HEX_INSTRUCTION_SIZE;
        }
        else if(!decodeType().isDynamic() && !decodeType().isList())
        {
            // Static basic type
            std::string paramValue;
            if(abiOutBasic(abiType, data, pos, paramValue))
            {
                value.push_back(paramValue);
            }
            else
            {
                return false;
            }
        }
        else if(decodeType().isDynamic() && decodeType().isList())
        {
            // Dynamic list type

            // Get position
            std::string paramValue;
            if(!abiOutBasic(ParameterType::abi_uint, data, pos, paramValue))
                return false;
            size_t oldPos = pos;
            pos = std::atoi(paramValue.c_str()) * 2;

            // Get length
            if(!abiOutBasic(ParameterType::abi_uint, data, pos, paramValue))
                return false;
            size_t length = std::atoi(paramValue.c_str());

            // Read list
            for(size_t i = 0; i < length; i++)
            {
                if(!abiOutBasic(abiType, data, pos, paramValue))
                    return false;
                value.push_back(paramValue);
            }

            // Restore position
            pos = oldPos;
        }
        else if(!decodeType().isDynamic() && decodeType().isList())
        {
            // Static list type
            std::string paramValue;
            size_t length = decodeType().length();

            // Read list
            for(size_t i = 0; i < length; i++)
            {
                if(!abiOutBasic(abiType, data, pos, paramValue))
                    return false;
                value.push_back(paramValue);
            }
        }
        else
        {
            // Unknown type
            m_lastError = UnsupportedABI;
            return false;
        }
    }
    catch(...)
    {
        m_lastError = DecodingError;
        return false;
    }

    return true;
}

bool ParameterABI::getRegularExpession(const ParameterType &paramType, QRegularExpression &regEx)
{
    bool ret = false;
    switch (paramType.type()) {
    case ParameterType::abi_bytes:
    {
        if(paramType.isDynamic())
        {
            regEx.setPattern(paternBytes);
        }
        else
        {
            // Expression to check the number of bytes encoded in hex (1-32)
            regEx.setPattern(QString(paternBytes32).arg(paramType.totalBytes()*2));
        }
        ret = true;
        break;
    }
    case ParameterType::abi_uint:
    {
        regEx.setPattern(paternUint);
        ret = true;
        break;
    }
    case ParameterType::abi_int:
    {
        regEx.setPattern(paternInt);
        ret = true;
        break;
    }
    case ParameterType::abi_address:
    {
        regEx.setPattern(paternAddress);
        ret = true;
        break;
    }
    case ParameterType::abi_bool:
    {
        regEx.setPattern(paternBool);
        ret = true;
        break;
    }
    default:
    {
        ret = false;
        break;
    }
    }
    return ret;
}

ParameterABI::ErrorType ParameterABI::lastError() const
{
    return m_lastError;
}

const ParameterType &ParameterABI::decodeType() const
{
    if(m_decodeType.canonical() != type)
    {
        m_decodeType = ParameterType(type);
    }

    return m_decodeType;
}

ParameterType::ParameterType(const std::string& _type):
    m_type(ParameterType::abi_none),
    m_whole(0),
    m_decimal(0),
    m_length(0),
    m_isList(false),
    m_valid(false)
{
    determine(_type);
}

bool ParameterType::determine(const std::string &_type)
{
    clean();

    // Initialize variables
    bool ret = true;
    size_t pos = 0;

    // Set string representation
    m_canonical = _type;

    // Determine the basic type
    if(startsWithString(m_canonical, "uint", pos))
    {
        m_type = abi_uint;
    }
    else if(startsWithString(m_canonical, "int", pos))
    {
        m_type = abi_int;
    }
    else if(startsWithString(m_canonical, "address", pos))
    {
        m_type = abi_address;
    }
    else if(startsWithString(m_canonical, "bool", pos))
    {
        m_type = abi_bool;
    }
    else if(startsWithString(m_canonical, "fixed", pos))
    {
        m_type = abi_fixed;
    }
    else if(startsWithString(m_canonical, "ufixed", pos))
    {
        m_type = abi_ufixed;
    }
    else if(startsWithString(m_canonical, "bytes", pos))
    {
        m_type = abi_bytes;
    }
    else if(startsWithString(m_canonical, "string", pos))
    {
        m_type = abi_string;
    }

    // Provide more informations about the type
    if(m_type != abi_none)
    {
        // Get the whole number part size
        std::string strWhole = startsWithNumber(m_canonical, pos);
        if(!strWhole.empty())
        {
            m_whole = atoi(strWhole.c_str());
        }

        // Get the decimal number part size
        if(startsWithString(m_canonical, "x", pos))
        {
            std::string strDecimal = startsWithNumber(m_canonical, pos);
            if(!strDecimal.empty())
            {
                m_decimal = atoi(strDecimal.c_str());
            }
            else
            {
                ret = false;
            }
        }

        // Get information for list type
        if(startsWithString(m_canonical, "[", pos))
        {
            std::string strLength = startsWithNumber(m_canonical, pos);
            if(!strLength.empty())
            {
                m_length = atoi(strLength.c_str());
            }
            if(startsWithString(m_canonical, "]", pos))
            {
                m_isList = true;
            }
            else
            {
                ret = false;
            }
        }
    }

    // Return if not parsed correctly
    if(m_canonical.length() != pos || ret == false)
        ret = false;

    // Check the validity of the types
    if(ret && (m_type == abi_int || m_type == abi_uint))
    {
        ret &= m_whole > 0;
        ret &= m_whole <= 256;
        ret &= m_whole % 8 == 0;
        ret &= m_decimal == 0;
    }

    if(ret && m_type == abi_address)
    {
        ret &= m_whole == 0;
        ret &= m_decimal == 0;
        if(ret) m_whole = 160;
    }

    if(ret && (m_type == abi_fixed || m_type == abi_ufixed))
    {
        ret &= m_whole > 0;
        ret &= m_decimal > 0;
        ret &= (m_whole + m_decimal) <= 256;
    }

    if(ret && m_type == abi_bytes)
    {
        m_whole *= 8;
        ret &= m_whole > 0;
        ret &= m_whole <= 256;
        ret &= m_decimal == 0;
    }

    if(ret && m_type == abi_function)
    {
        ret &= m_whole == 0;
        ret &= m_decimal == 0;
        if(ret) m_whole = 196;
    }

    if(ret && m_type == abi_string)
    {
        ret &= m_whole == 0;
        ret &= m_decimal == 0;
    }

    m_valid = ret;

    return m_valid;
}

size_t ParameterType::wholeBits() const
{
    return m_whole;
}

size_t ParameterType::decimalBits() const
{
    return m_decimal;
}

size_t ParameterType::totalBytes() const
{
    return ceil((m_whole + m_decimal) / 8.0);
}

size_t ParameterType::length() const
{
    return m_length;
}

bool ParameterType::isList() const
{
    return m_isList;
}

bool ParameterType::isDynamic() const
{
    // Type bytes is dynamic when the count of bytes is not known.
    // Type string is always dynamic.
    if((m_type == abi_bytes && totalBytes() == 0) || m_type == abi_string)
    {
        return true;
    }

    // Type list is dynamic when the count of elements is not known.
    if(m_isList)
        return m_length == 0;

    return false;
}

bool ParameterType::isValid() const
{
    return m_valid;
}

const std::string& ParameterType::canonical() const
{
    return m_canonical;
}

void ParameterType::clean()
{
    m_type = ParameterType::abi_none;
    m_whole = 0;
    m_decimal = 0;
    m_length = 0;
    m_isList = false;
    m_valid = false;
    m_canonical = "";
}

ParameterType::Type ParameterType::type() const
{
    return m_type;
}
