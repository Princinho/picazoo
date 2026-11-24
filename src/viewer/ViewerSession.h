#pragma once

// Contrôleur QML de la visionneuse autonome : parcours du dossier, photo courante,
// informations, réglages, proposition d'association, passerelles vers Windows et
// vers picazoo.exe. Ne touche jamais la bibliothèque.

#include "browse/FolderBrowser.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QThreadPool>
#include <QPointF>
#include <QPointer>
#include <QScreen>
#include <QSize>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class FileImageProvider;
class ViewerSettings;

class ViewerSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentChanged)
    Q_PROPERTY(QString currentName READ currentName NOTIFY currentChanged)
    Q_PROPERTY(QString currentKind READ currentKind NOTIFY currentChanged)
    Q_PROPERTY(int direction READ direction NOTIFY currentChanged)
    Q_PROPERTY(QString positionText READ positionText NOTIFY positionChanged)
    Q_PROPERTY(int count READ count NOTIFY positionChanged)
    Q_PROPERTY(int index READ index NOTIFY positionChanged)
    Q_PROPERTY(QVariantList neighbors READ neighbors NOTIFY positionChanged)
    Q_PROPERTY(QVariantMap info READ info NOTIFY infoChanged)
    Q_PROPERTY(bool opaque READ opaque WRITE setOpaque NOTIFY opaqueChanged)
    Q_PROPERTY(bool stripVisible READ stripVisible WRITE setStripVisible NOTIFY stripVisibleChanged)
    Q_PROPERTY(bool animations READ animations CONSTANT)
    Q_PROPERTY(bool transparencyAvailable READ transparencyAvailable CONSTANT)
    Q_PROPERTY(bool proposalVisible READ proposalVisible NOTIFY proposalVisibleChanged)
    Q_PROPERTY(bool registered READ registered NOTIFY registeredChanged)
    Q_PROPERTY(QScreen* screen READ screen CONSTANT)

public:
    ViewerSession(FileImageProvider& images, ViewerSettings& settings, QScreen* screen, QObject* parent = nullptr);
    ~ViewerSession() override;

    // Ouvre une photo ; `foreground` : fenêtre au premier plan au moment de la demande
    // (pour retrouver la fenêtre de l'Explorateur et son ordre).
    void open(const QString& path, quintptr foreground);
    // Message d'une autre instance : ignoré s'il suit de moins de 500 ms une ouverture.
    void openFromMessage(const QString& path, quintptr foreground);

    QString currentPath() const { return browser_.currentPath(); }
    QString currentName() const;
    QString currentKind() const;
    int direction() const { return direction_; }
    QString positionText() const;
    int count() const { return int(browser_.count()); }
    int index() const { return browser_.currentIndex(); }
    QVariantList neighbors() const;
    QVariantMap info() const { return info_; }
    bool opaque() const;
    void setOpaque(bool v);
    bool stripVisible() const;
    void setStripVisible(bool v);
    bool animations() const { return animations_; }
    bool transparencyAvailable() const { return transparency_; }
    bool proposalVisible() const { return proposalVisible_; }
    bool registered() const;
    QScreen* screen() const { return screen_; }
    picazoo::FolderBrowser& browser() { return browser_; }

    // Images et parcours
    Q_INVOKABLE QString source(const QString& kind, const QString& path) const;
    Q_INVOKABLE QString kindOf(const QString& path) const;
    Q_INVOKABLE QString pathAt(int delta) const { return browser_.pathAt(delta); }
    Q_INVOKABLE QSize fullSize(const QString& path) const;
    Q_INVOKABLE QString decodeError(const QString& path) const;
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void first();
    Q_INVOKABLE void last();
    Q_INVOKABLE void goTo(int index);

    // Zoom (research V10, browse/ZoomMath)
    Q_INVOKABLE double fitScale(double iw, double ih, double vw, double vh) const;
    Q_INVOKABLE double clampScale(double s, double iw, double ih, double vw, double vh) const;
    Q_INVOKABLE double wheelFactor(double angleDeltaY) const;
    Q_INVOKABLE QPointF zoomAround(double ox, double oy, double scale, double newScale, double ax, double ay) const;
    Q_INVOKABLE QPointF clampOffset(double ox, double oy, double scale, double iw, double ih, double vw,
                                    double vh) const;
    Q_INVOKABLE QPointF anchorOnImage(double ax, double ay, double ox, double oy, double scale, double iw,
                                      double ih) const;
    Q_INVOKABLE QPointF zoomEndOffset(double ox, double oy, double scale, double newScale, double ax, double ay,
                                      double iw, double ih, double vw, double vh) const;
    Q_INVOKABLE QPointF zoomStepOffset(double sx, double sy, double startScale, double endScale, double ex,
                                       double ey, double current, double ax, double ay) const;
    Q_INVOKABLE QPointF clampPan(double ox, double oy, double prevX, double prevY, double scale, double iw,
                                 double ih, double vw, double vh) const;

    // Actions
    Q_INVOKABLE void revealInExplorer();
    Q_INVOKABLE void openWithOther();
    Q_INVOKABLE void openWithDefault();
    Q_INVOKABLE void openInPicazoo();
    Q_INVOKABLE void firstImageShown();

    // Association (FR-029 à FR-032)
    Q_INVOKABLE QVariantList formatFamilies() const;
    Q_INVOKABLE QVariantList associationStatus() const;
    Q_INVOKABLE void answerProposal(const QString& answer, const QStringList& families = {});
    Q_INVOKABLE void showProposal();
    Q_INVOKABLE void hideProposal();
    Q_INVOKABLE bool registerNow();
    Q_INVOKABLE QStringList extensionsFor(const QStringList& families) const;

signals:
    void currentChanged();
    void positionChanged();
    void infoChanged();
    void opaqueChanged();
    void stripVisibleChanged();
    void proposalVisibleChanged();
    void registeredChanged();
    void wrapped(bool forward);
    void currentRemoved();
    void raiseRequested();
    void message(const QString& text);
    void quitRequested();

private:
    void onCurrentChanged();
    void prefetch();
    void loadInfo();
    bool shouldPropose() const;
    qint64 stampOf(const QString& path) const;
    QString viewerExe() const;

    FileImageProvider& images_;
    ViewerSettings& settings_;
    QPointer<QScreen> screen_;
    picazoo::FolderBrowser browser_;
    quintptr foreground_ = 0;
    int direction_ = 0;
    QVariantMap info_;
    quint64 infoGen_ = 0;
    bool animations_ = true;
    bool transparency_ = true;
    bool proposalVisible_ = false;
    bool firstShown_ = false;
    QElapsedTimer sinceOpen_;
    QHash<QString, qint64> stamps_;
    QThreadPool infoPool_;
};
