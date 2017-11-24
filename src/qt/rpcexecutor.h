#ifndef RPCEXECUTOR_H
#define RPCEXECUTOR_H

#include <QObject>
#include <QString>

/* Object for executing console RPC commands in a separate thread.
*/
class RPCExecutor : public QObject
{
    Q_OBJECT

public slots:
    void start();
    void request(const QString &command);

signals:
    void reply(int category, const QString &command);
};

#endif // RPCEXECUTOR_H
