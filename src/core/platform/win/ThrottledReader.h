#pragma once

// Lecture complète et régulée d'un fichier de l'utilisateur pour calculer son empreinte de
// contenu (research D1, D5, D6). Lecture seule et partagée, sans passer par le cache de
// Windows (FILE_FLAG_NO_BUFFERING), par blocs alignés de 1 Mo, au débit fixé par un
// IoGovernor. Un fichier « disponible uniquement en ligne » n'est jamais ouvert.

#include <QByteArray>
#include <QString>

#include <atomic>
#include <functional>

namespace picazoo::platform {

class IoGovernor;

enum class ReadStatus { Ok, OnlineOnly, Missing, Failed, Cancelled };

struct ContentHash {
    ReadStatus status = ReadStatus::Failed;
    QByteArray hash; // XXH3-128, forme canonique (16 octets) si Ok
    qint64 bytesRead = 0;
};

struct ReadControl {
    IoGovernor* governor = nullptr;          // nul : pas de plafond
    std::function<bool()> cancelled;         // vrai : abandon au prochain bloc
    std::function<void()> waitWhileHeld;     // bloque tant que la lecture doit attendre (pause…)
};

// Vrai si le fichier n'est présent qu'en ligne (OneDrive et équivalents) : le lire
// déclencherait son téléchargement. Ne lit que les attributs.
bool isOnlineOnly(const QString& path);

ContentHash hashFileContent(const QString& path, const ReadControl& control = {});

// Compteurs globaux (tests de reprise SC-007, banc d'essai).
struct ReadCounters {
    std::atomic<qint64> files{0};
    std::atomic<qint64> bytes{0};
};
ReadCounters& readCounters();

} // namespace picazoo::platform
