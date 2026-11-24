// T025/T026 (005) — inscription auprès de Windows, jamais imposée
// (contracts/storage-and-registry.md). Tout se passe sous une clé de test de HKCU,
// supprimée à la fin : le registre réel de l'utilisateur n'est pas touché.

#include "platform/win/FileAssociations.h"

#include <windows.h>

#include <QUuid>
#include <QtTest>

using namespace picazoo::platform;

namespace {

std::wstring w(const QString& s) { return s.toStdWString(); }

bool exists(const QString& path)
{
    HKEY k = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, w(path).c_str(), 0, KEY_READ, &k) != ERROR_SUCCESS)
        return false;
    RegCloseKey(k);
    return true;
}

bool hasValue(const QString& path, const QString& name)
{
    return RegGetValueW(HKEY_CURRENT_USER, w(path).c_str(), w(name).c_str(), RRF_RT_ANY, nullptr, nullptr, nullptr) ==
           ERROR_SUCCESS;
}

QString stringValue(const QString& path, const QString& name)
{
    wchar_t buf[512] = {};
    DWORD size = sizeof(buf);
    if (RegGetValueW(HKEY_CURRENT_USER, w(path).c_str(), name.isEmpty() ? nullptr : w(name).c_str(), RRF_RT_REG_SZ,
                     nullptr, buf, &size) != ERROR_SUCCESS)
        return {};
    return QString::fromWCharArray(buf);
}

void setString(const QString& path, const QString& name, const QString& value)
{
    HKEY k = nullptr;
    RegCreateKeyExW(HKEY_CURRENT_USER, w(path).c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr);
    const std::wstring v = w(value);
    RegSetValueExW(k, name.isEmpty() ? nullptr : w(name).c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(v.c_str()),
                   DWORD((v.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
}

} // namespace

class TstAssociations : public QObject {
    Q_OBJECT

    QString root_;

private slots:
    void init()
    {
        root_ = QStringLiteral("Software\\PicazooTest-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
        qputenv("PICAZOO_REGISTRY_ROOT", root_.toUtf8());
    }

    void cleanup()
    {
        RegDeleteTreeW(HKEY_CURRENT_USER, w(root_).c_str());
        RegDeleteKeyW(HKEY_CURRENT_USER, w(root_).c_str());
        qunsetenv("PICAZOO_REGISTRY_ROOT");
    }

    void registerWritesOnlyTheContract()
    {
        const QString classes = root_ + QStringLiteral("\\Classes");
        // État préexistant : une autre application ouvre les JPEG et figure dans « Ouvrir avec ».
        setString(classes + QStringLiteral("\\.jpg"), {}, QStringLiteral("AutreVisionneuse.jpg"));
        setString(classes + QStringLiteral("\\.jpg\\OpenWithProgids"), QStringLiteral("AutreVisionneuse.jpg"), {});

        QVERIFY(FileAssociations::registerApp(RegistryScope::User, QStringLiteral("C:\\Picazoo\\bin\\picazoo-viewer.exe")));
        QVERIFY(FileAssociations::isRegistered());
        QCOMPARE(stringValue(classes + QStringLiteral("\\Picazoo.Image\\shell\\open\\command"), {}),
                 QStringLiteral("\"C:\\Picazoo\\bin\\picazoo-viewer.exe\" \"%1\""));
        // Les miniatures de l'Explorateur restent celles de Windows quand Picazoo est par défaut.
        QCOMPARE(stringValue(classes + QStringLiteral("\\Picazoo.Image\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}"), {}),
                 QStringLiteral("{C7657C4A-9F68-40fa-A4DF-96BC08EB3551}"));
        for (const QString& ext : FileAssociations::extensions()) {
            QVERIFY2(hasValue(classes + u'\\' + ext + QStringLiteral("\\OpenWithProgids"), QStringLiteral("Picazoo.Image")),
                     qPrintable(ext));
            QCOMPARE(stringValue(root_ + QStringLiteral("\\Picazoo\\Capabilities\\FileAssociations"), ext),
                     QStringLiteral("Picazoo.Image"));
        }
        QCOMPARE(stringValue(root_ + QStringLiteral("\\RegisteredApplications"), QStringLiteral("Picazoo")),
                 root_ + QStringLiteral("\\Picazoo\\Capabilities"));
        // Jamais imposé : la valeur par défaut de l'extension n'a pas changé.
        QCOMPARE(stringValue(classes + QStringLiteral("\\.jpg"), {}), QStringLiteral("AutreVisionneuse.jpg"));
        QVERIFY(!exists(classes + QStringLiteral("\\.jpg\\UserChoice")));

        QVERIFY(FileAssociations::unregisterApp(RegistryScope::User));
        QVERIFY(!exists(classes + QStringLiteral("\\Picazoo.Image")));
        QVERIFY(!exists(classes + QStringLiteral("\\Applications\\picazoo-viewer.exe")));
        QVERIFY(!exists(root_ + QStringLiteral("\\Picazoo")));
        QVERIFY(!hasValue(root_ + QStringLiteral("\\RegisteredApplications"), QStringLiteral("Picazoo")));
        QVERIFY(!exists(classes + QStringLiteral("\\.png"))); // créée par l'inscription, retirée
        // Ce qui appartenait à l'autre application est intact.
        QCOMPARE(stringValue(classes + QStringLiteral("\\.jpg"), {}), QStringLiteral("AutreVisionneuse.jpg"));
        QVERIFY(hasValue(classes + QStringLiteral("\\.jpg\\OpenWithProgids"), QStringLiteral("AutreVisionneuse.jpg")));
        QVERIFY(!hasValue(classes + QStringLiteral("\\.jpg\\OpenWithProgids"), QStringLiteral("Picazoo.Image")));
    }
};

QTEST_GUILESS_MAIN(TstAssociations)
#include "tst_associations.moc"
