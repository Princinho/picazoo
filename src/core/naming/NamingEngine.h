#pragma once

// Moteur des images liées (research.md N3) : aucun fil propre, tout passe par Library::post
// (thread d'indexation). Un repérage complet après la première analyse, puis seulement les
// dossiers touchés : un élément ajouté, renommé ou retiré ne fait recalculer que sa racine.
// Ne lit que les noms déjà indexés par la 001, jamais un fichier de l'utilisateur.

#include "library/Types.h"

#include <QObject>
#include <QStringList>
#include <QTimer>

#include <memory>

namespace picazoo {

class Library;
class NamingStore;

class NamingEngine : public QObject {
    Q_OBJECT

public:
    // `store` doit survivre au moteur ; le moteur est détruit avant la bibliothèque.
    NamingEngine(Library& library, NamingStore& store, QObject* parent = nullptr);
    ~NamingEngine() override;

    void rescanAll(); // « Rechercher maintenant »

    NamingStore& store() { return store_; }
    int fullScanCount() const; // repérages complets effectués
    // Banc d'essai : durée du plus long passage sur le thread d'indexation depuis le dernier appel.
    qint64 takeSlowestPassMs();

    // Tests et banc d'essai : traite les événements jusqu'à ce que les groupes soient à jour.
    bool waitForIdle(int timeoutMs);
    bool idle() const;

signals:
    void groupsChanged();

private:
    struct State;

    void queueFolder(qint64 folder);
    void schedule();
    void flush();

    Library& lib_;
    NamingStore& store_;
    std::shared_ptr<State> state_; // partagé avec les tâches du thread d'indexation
    QTimer batchTimer_;            // regroupe les dossiers touchés avant de les recalculer
    QTimer notifyTimer_;
    bool fullRequested_ = false;
};

} // namespace picazoo
