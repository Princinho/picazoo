#pragma once

// Instance unique d'un exécutable de Picazoo (research V4, contracts/command-line.md).
// Un mutex nommé élit l'instance principale ; les suivantes lui transmettent un
// message (un chemin) par un tube local propre à la session, puis quittent.

#include <QObject>
#include <QString>

#include <memory>

class QLocalServer;

namespace picazoo::platform {

class SingleInstance : public QObject {
    Q_OBJECT

public:
    // `key` : « Picazoo.Viewer » ou « Picazoo.App ».
    explicit SingleInstance(const QString& key, QObject* parent = nullptr);
    ~SingleInstance() override;

    bool isPrimary() const { return primary_; }

    // Instance principale : commence à écouter. Faux si le tube ne peut pas être créé.
    bool listen();

    // Instance secondaire : transmet `message` (sans retour à la ligne) à l'instance
    // principale et l'autorise à passer au premier plan. Réessaie jusqu'à `timeoutMs`
    // pendant que l'instance principale démarre.
    bool sendToPrimary(const QString& message, int timeoutMs = 2000);

    QString serverName() const { return serverName_; }

signals:
    void messageReceived(const QString& message);

private:
    void onNewConnection();

    QString serverName_;
    void* mutex_ = nullptr;
    bool primary_ = false;
    std::unique_ptr<QLocalServer> server_;
};

} // namespace picazoo::platform
