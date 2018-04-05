#ifndef CONTRACTABI_H
#define CONTRACTABI_H
#include <string>
#include <vector>
#include <map>
#include <QRegularExpression>
#include <QStringList>

#define paternUint "^[0-9]{1,77}$"
#define paternInt "^\\-{0,1}[0-9]{1,76}$"
#define paternAddress "^[a-fA-F0-9]{40,40}$"
#define paternBool "^true$|^false$"
#define paternHex "^[a-fA-F0-9]{1,}$"
#define paternBytes paternHex
#define paternBytes32 "^[a-fA-F0-9]{%1,%1}$"


/**
 * @brief The ParameterType class Decode the api parameter type,
 * provide more informations about the data type that can be used for encoding and decoding.
 */
class ParameterType
{
public:
    /**
     * @brief The Type enum ABI data types
     */
    enum Type
    {
        abi_none,
        abi_bytes,
        abi_string,
        abi_uint,
        abi_int,
        abi_address,
        abi_bool,
        abi_fixed,
        abi_ufixed,
        abi_function
    };

    /**
     * @brief ParameterType Constructor
     */
    ParameterType(const std::string& _type = "");

    /**
     * @brief determine Determine the type from the string
     * @param _type String representation of the type
     * @return true: type determined, false: type not determined
     */
    bool determine(const std::string& _type);

    /**
     * @brief wholeBits Get the number of bits for the whole part of the number
     * @return Number of bits
     */
    size_t wholeBits() const;

    /**
     * @brief decimalBits Get the number of bits for the decimal part of the number
     * @return Number of bits
     */
    size_t decimalBits() const;

    /**
     * @brief totalBytes Get the total size in bytes
     * @return Number of bytes
     */
    size_t totalBytes() const;

    /**
     * @brief length Length of the list, applicable for list only
     * @return Length of the list
     */
    size_t length() const;

    /**
     * @brief isList Is the data type list
     * @return true: is list, false: not list
     */
    bool isList() const;

    /**
     * @brief isDynamic Check if the type is dynamic.
     * Dynamic type include pointer to the location where the content starts.
     * @return true: is dynamic, false: is static
     */
    bool isDynamic() const;

    /**
     * @brief isValid Check if the type is valid
     * @return true: is valid, false: not valid
     */
    bool isValid() const;

    /**
     * @brief canonical Get the canonical type
     * @return String representation of the type
     */
    const std::string& canonical() const;

    /**
     * @brief type Get the type
     * @return Decoded type
     */
    Type type() const;

private:
    /**
     * @brief clean Set the value to the defaults
     */
    void clean();

    Type m_type;
    int m_whole;
    int m_decimal;
    int m_length;
    bool m_isList;
    bool m_valid;
    std::string m_canonical;
};

class ParameterABI
{
public:
    enum ErrorType
    {
        Ok = 0,
        UnsupportedABI,
        EncodingError,
        DecodingError
    };

    ParameterABI(const std::string& _name = "", const std::string& _type = "", bool _indexed = false);
    ~ParameterABI();
    bool abiIn(const std::vector<std::string> &value, std::string &data, std::map<int, std::string>& mapDynamic) const;
    bool abiOut(const std::string &data, size_t& pos, std::vector<std::string> &value) const;
    const ParameterType &decodeType() const;
    static bool getRegularExpession(const ParameterType &paramType, QRegularExpression &regEx);

    std::string name; // The name of the parameter;
    std::string type; // The canonical type of the parameter.
    bool indexed; // True if the field is part of the log's topics, false if it one of the log's data segment.
    // Indexed is only used with event function

    ErrorType lastError() const;

private:
    bool abiInBasic(ParameterType::Type abiType, std::string value, std::string &data) const;
    bool abiOutBasic(ParameterType::Type abiType, const std::string &data, size_t &pos, std::string &value) const;
    void addDynamic(const std::string& paramData, std::string &data, std::map<int, std::string>& mapDynamic) const;


    mutable ParameterType m_decodeType;
    mutable ErrorType m_lastError;
};

class FunctionABI
{
public:
    FunctionABI(const std::string& _name = "",
                const std::string& _type = "function",
                const std::vector<ParameterABI>& _inputs = std::vector<ParameterABI>(),
                const std::vector<ParameterABI>& _outputs = std::vector<ParameterABI>(),
                bool _payable = false,
                bool _constant = false,
                bool _anonymous = false);

    bool abiIn(const std::vector<std::vector<std::string>>& values, std::string& data, std::vector<ParameterABI::ErrorType>& errors) const;

    bool abiOut(const std::string& data, std::vector<std::vector<std::string>>& values, std::vector<ParameterABI::ErrorType>& errors) const;

    std::string selector() const;

    static std::string defaultSelector();

    QString errorMessage(std::vector<ParameterABI::ErrorType>& errors, bool in) const;

    std::string name; // The name of the function;
    std::string type; // Function types: "function", "constructor", "fallback" or "event"
    std::vector<ParameterABI> inputs; // Array of input parameters
    std::vector<ParameterABI> outputs; // Array of output parameters, can be omitted if function doesn't return
    bool payable; // True if function accepts ether, defaults to false.
    bool constant; // True if function is specified to not modify blockchain state.
    bool anonymous; // True if the event was declared as anonymous.

    // Constructor and fallback function never have name or outputs.
    // Fallback function doesn't have inputs either.
    // Event function is the only one that have anonymous.
    // Sending non-zero ether to non-payable function will throw.
    // Type can be omitted, defaulting to "function".

private:
    void processDynamicParams(const std::map<int, std::string>& mapDynamic, std::string& data) const;
};

class ContractABI
{
public:
    ContractABI();
    bool loads(const std::string& json_data);
    void clean();

    std::vector<FunctionABI> functions;
};

#endif // CONTRACTABI_H
