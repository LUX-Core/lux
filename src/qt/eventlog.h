#ifndef EVENTLOG_H
#define EVENTLOG_H
#include <string>
#include <vector>
#include <QMap>
#include <QVariant>

class ExecRPCCommand;

class EventLog
{
public:
    /**
     * @brief EventLog Constructor
     */
    EventLog();

    /**
     * @brief ~EventLog Destructor
     */
    ~EventLog();

    /**
     * @brief search
     * @param fromBlock
     * @param toBlock
     * @param eventName
     * @param addresses
     * @param result
     * @return
     */
    bool search(int fromBlock, int toBlock, const std::string& eventName, const std::vector<std::string>& addresses, QVariant& result);

private:
    // Set command data
    void setStartBlock(int fromBlock);
    void setEndBlock(int toBlock);
    void setAddresses(const std::vector<std::string> addresses);
    void setEventName(const std::string &eventName);

    ExecRPCCommand* m_RPCCommand;
    QMap<QString, QString> m_lstParams;
};

#endif // EVENTLOG_H

