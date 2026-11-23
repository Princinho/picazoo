#include "platform/win/SingleInstance.h"

#include <windows.h>

#include <QDeadlineTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QThread>

namespace picazoo::platform {

namespace {

QString sessionSuffix()
{
    DWORD session = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &session);
    wchar_t user[256] = {};
    DWORD len = DWORD(std::size(user));
    GetUserNameW(user, &len);
    return QStringLiteral("%1.%2").arg(session).arg(QString::fromWCharArray(user));
}

} // namespace

SingleInstance::SingleInstance(const QString& key, QObject* parent)
    : QObject(parent), serverName_(key + u'.' + sessionSuffix())
{
    const std::wstring name = (QStringLiteral("Local\\") + key).toStdWString();
    mutex_ = CreateMutexW(nullptr, FALSE, name.c_str());
    primary_ = mutex_ && GetLastError() != ERROR_ALREADY_EXISTS;
}

SingleInstance::~SingleInstance()
{
    if (server_)
        server_->close();
    if (mutex_)
        CloseHandle(static_cast<HANDLE>(mutex_));
}

bool SingleInstance::listen()
{
    server_ = std::make_unique<QLocalServer>();
    server_->setSocketOptions(QLocalServer::UserAccessOption);
    if (!server_->listen(serverName_)) {
        QLocalServer::removeServer(serverName_);
        if (!server_->listen(serverName_))
            return false;
    }
    connect(server_.get(), &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);
    return true;
}

void SingleInstance::onNewConnection()
{
    while (QLocalSocket* socket = server_->nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        auto buffer = std::make_shared<QByteArray>();
        auto read = [this, socket, buffer] {
            buffer->append(socket->readAll());
            qsizetype nl;
            while ((nl = buffer->indexOf('\n')) >= 0) {
                const QString message = QString::fromUtf8(buffer->left(nl)).trimmed();
                buffer->remove(0, nl + 1);
                if (message.size() < 32768)
                    emit messageReceived(message);
            }
        };
        connect(socket, &QLocalSocket::readyRead, this, read);
        if (socket->bytesAvailable() > 0)
            read();
    }
}

bool SingleInstance::sendToPrimary(const QString& message, int timeoutMs)
{
    AllowSetForegroundWindow(ASFW_ANY);
    QDeadlineTimer deadline(timeoutMs);
    while (true) {
        QLocalSocket socket;
        socket.connectToServer(serverName_);
        if (socket.waitForConnected(200)) {
            const QByteArray data = message.toUtf8() + '\n';
            bool ok = socket.write(data) == data.size();
            socket.flush();
            // waitForBytesWritten() est faux quand tout est déjà parti : on ne l'appelle
            // que s'il reste des octets en attente.
            if (ok && socket.bytesToWrite() > 0)
                ok = socket.waitForBytesWritten(1000);
            socket.disconnectFromServer();
            if (socket.state() != QLocalSocket::UnconnectedState)
                socket.waitForDisconnected(500);
            return ok;
        }
        if (deadline.hasExpired())
            return false;
        QThread::msleep(50);
    }
}

} // namespace picazoo::platform
