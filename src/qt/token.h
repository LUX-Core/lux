#ifndef TOKEN_H
#define TOKEN_H
#include <string>
#include <vector>

struct TokenEvent{
    std::string m_address;
    std::string m_sender;
    std::string m_receiver;
    std::string m_value;

    TokenEvent(std::string address, std::string sender, std::string receiver, std::string value){
        m_address = address;
        m_sender = sender;
        m_receiver = receiver;
        m_value = value;
    }
};

struct TokenData;

class Token
{
public:
    Token();
    ~Token();

    // Set command data
    void setAddress(const std::string &address);
    void setDataHex(const std::string &datahex);
    void setAmount(const std::string &amount);
    void setGasLimit(const std::string &gaslimit);
    void setGasPrice(const std::string &gasPrice);
    void setSender(const std::string &sender);
    void clear();

    // ABI Functions
    bool name(std::string& result, bool sendTo = false);
    bool approve(const std::string& _spender, const std::string& _value, bool& success, bool sendTo = false);
    bool totalSupply(std::string& result, bool sendTo = false);
    bool transferFrom(const std::string& _from, const std::string& _to, const std::string& _value, bool& success, bool sendTo = false);
    bool decimals(std::string& result, bool sendTo = false);
    bool burn(const std::string& _value, bool& success, bool sendTo = false);
    bool balanceOf(std::string& result, bool sendTo = false);
    bool balanceOf(const std::string& spender, std::string& result, bool sendTo = false);
    bool burnFrom(const std::string& _from, const std::string& _value, bool& success, bool sendTo = false);
    bool symbol(std::string& result, bool sendTo = false);
    bool transfer(const std::string& _to, const std::string& _value, bool sendTo = false);
    bool approveAndCall(const std::string& _spender, const std::string& _value, const std::string& _extraData, bool& success, bool sendTo = false);
    bool allowance(const std::string& _from, const std::string& _to, std::string& result, bool sendTo = false);

    // ABI Events
    void listenForAddresses(const std::vector<std::string>& contractAddresses);
    bool transferEvents(int fromBlock, int toBlock, std::vector<TokenEvent>& tokenEvents);
    bool burnEvents(int fromBlock, int toBlock, std::vector<TokenEvent>& tokenEvents);

private:
    bool exec(const std::vector<std::string>& input, int func, std::vector<std::string>& output, bool sendTo);
    bool execEvents(int fromBlock, int toBlock, int func, std::vector<TokenEvent> &tokenEvents);

    Token(Token const&);
    Token& operator=(Token const&);

private:
    TokenData* d;
};

#endif // TOKEN_H
