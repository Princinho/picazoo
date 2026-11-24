#include "platform/win/FileAssociations.h"

#include <windows.h>

#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <QDir>
#include <QFileInfo>

#include <string>

namespace picazoo::platform {

namespace {

constexpr const wchar_t* kExeName = L"picazoo-viewer.exe";
// IID de IThumbnailProvider, sous lequel l'Explorateur cherche le gestionnaire de miniatures.
constexpr const wchar_t* kThumbnailHandlerKey = L"{e357fccd-a995-4576-b01f-234630154e96}";
// « Windows Photo Thumbnail Provider » (PhotoMetadataHandler.dll).
constexpr const wchar_t* kWindowsPhotoThumbnailProvider = L"{C7657C4A-9F68-40fa-A4DF-96BC08EB3551}";

std::wstring w(const QString& s) { return s.toStdWString(); }

HKEY rootOf(RegistryScope scope) { return scope == RegistryScope::Machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER; }

bool setString(HKEY root, const QString& path, const wchar_t* name, const QString& value)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(root, w(path).c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;
    const std::wstring v = w(value);
    const LSTATUS st = RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(v.c_str()),
                                      DWORD((v.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return st == ERROR_SUCCESS;
}

bool keyExists(HKEY root, const QString& path)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, w(path).c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS)
        return false;
    RegCloseKey(key);
    return true;
}

// Supprime la clé si elle n'a ni valeur ni sous-clé.
void deleteIfEmpty(HKEY root, const QString& path)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, w(path).c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS)
        return;
    DWORD subKeys = 0, values = 0;
    const LSTATUS st = RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, &subKeys, nullptr, nullptr, &values, nullptr,
                                        nullptr, nullptr, nullptr);
    RegCloseKey(key);
    if (st == ERROR_SUCCESS && subKeys == 0 && values == 0)
        RegDeleteKeyW(root, w(path).c_str());
}

void deleteTree(HKEY root, const QString& path)
{
    RegDeleteTreeW(root, w(path).c_str());
    RegDeleteKeyW(root, w(path).c_str());
}

QString testRoot() { return qEnvironmentVariable("PICAZOO_REGISTRY_ROOT"); }

} // namespace

QString FileAssociations::baseKey(RegistryScope scope)
{
    const QString t = testRoot();
    if (scope == RegistryScope::User && !t.isEmpty())
        return t;
    return QStringLiteral("Software");
}

QStringList FileAssociations::extensions()
{
    return {QStringLiteral(".jpg"),  QStringLiteral(".jpeg"), QStringLiteral(".jpe"),  QStringLiteral(".png"),
            QStringLiteral(".gif"),  QStringLiteral(".bmp"),  QStringLiteral(".tif"),  QStringLiteral(".tiff"),
            QStringLiteral(".webp"), QStringLiteral(".heic"), QStringLiteral(".heif"), QStringLiteral(".cr2"),
            QStringLiteral(".cr3"),  QStringLiteral(".nef"),  QStringLiteral(".arw"),  QStringLiteral(".orf"),
            QStringLiteral(".rw2"),  QStringLiteral(".raf"),  QStringLiteral(".dng")};
}

bool FileAssociations::registerApp(RegistryScope scope, const QString& viewerExe, QString* error)
{
    const HKEY root = rootOf(scope);
    const QString base = baseKey(scope);
    const QString classes = base + QStringLiteral("\\Classes");
    const QString exe = QDir::toNativeSeparators(viewerExe);
    const QString command = QStringLiteral("\"%1\" \"%2\"").arg(exe, QStringLiteral("%1"));
    const QString icon = QStringLiteral("\"%1\",0").arg(exe);
    const QString progId = QString::fromWCharArray(kProgId);
    const QString typeName = QStringLiteral("Photo (Picazoo)");

    bool ok = true;
    const QString prog = classes + u'\\' + progId;
    ok &= setString(root, prog, nullptr, typeName);
    ok &= setString(root, prog, L"FriendlyTypeName", typeName);
    ok &= setString(root, prog + QStringLiteral("\\DefaultIcon"), nullptr, icon);
    ok &= setString(root, prog + QStringLiteral("\\shell\\open\\command"), nullptr, command);
    // Sans gestionnaire de miniatures sur le ProgID, l'Explorateur affiche l'icône de Picazoo
    // au lieu de l'aperçu une fois Picazoo choisi par défaut : on reprend celui de Windows
    // (le même que « jpegfile »), qui passe par WIC et couvre donc tous les formats décodables.
    ok &= setString(root, prog + QStringLiteral("\\ShellEx\\") + QString::fromWCharArray(kThumbnailHandlerKey), nullptr,
                    QString::fromWCharArray(kWindowsPhotoThumbnailProvider));

    const QString app = classes + QStringLiteral("\\Applications\\") + QString::fromWCharArray(kExeName);
    ok &= setString(root, app, L"FriendlyAppName", QStringLiteral("Picazoo"));
    ok &= setString(root, app + QStringLiteral("\\DefaultIcon"), nullptr, icon);
    ok &= setString(root, app + QStringLiteral("\\shell\\open\\command"), nullptr, command);

    const QString caps = base + QStringLiteral("\\Picazoo\\Capabilities");
    ok &= setString(root, caps, L"ApplicationName", QStringLiteral("Picazoo"));
    ok &= setString(root, caps, L"ApplicationDescription",
                    QStringLiteral("Visionneuse de photos de Picazoo, rapide et légère."));
    ok &= setString(root, caps, L"ApplicationIcon", icon);

    for (const QString& ext : extensions()) {
        const std::wstring e = w(ext);
        ok &= setString(root, app + QStringLiteral("\\SupportedTypes"), e.c_str(), QString());
        ok &= setString(root, classes + u'\\' + ext + QStringLiteral("\\OpenWithProgids"), kProgId, QString());
        ok &= setString(root, caps + QStringLiteral("\\FileAssociations"), e.c_str(), progId);
    }
    ok &= setString(root, base + QStringLiteral("\\RegisteredApplications"), kAppName, caps);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    if (!ok && error)
        *error = QStringLiteral("Picazoo n'a pas pu s'inscrire auprès de Windows (droits insuffisants ?).");
    return ok;
}

bool FileAssociations::unregisterApp(RegistryScope scope, QString* error)
{
    Q_UNUSED(error);
    const HKEY root = rootOf(scope);
    const QString base = baseKey(scope);
    const QString classes = base + QStringLiteral("\\Classes");

    deleteTree(root, classes + u'\\' + QString::fromWCharArray(kProgId));
    deleteTree(root, classes + QStringLiteral("\\Applications\\") + QString::fromWCharArray(kExeName));
    deleteIfEmpty(root, classes + QStringLiteral("\\Applications"));
    for (const QString& ext : extensions()) {
        const QString owp = classes + u'\\' + ext + QStringLiteral("\\OpenWithProgids");
        RegDeleteKeyValueW(root, w(owp).c_str(), kProgId);
        deleteIfEmpty(root, owp);
        deleteIfEmpty(root, classes + u'\\' + ext);
    }
    deleteTree(root, base + QStringLiteral("\\Picazoo\\Capabilities"));
    deleteIfEmpty(root, base + QStringLiteral("\\Picazoo"));
    RegDeleteKeyValueW(root, w(base + QStringLiteral("\\RegisteredApplications")).c_str(), kAppName);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

bool FileAssociations::isRegistered()
{
    const QString cmd = QStringLiteral("\\Classes\\") + QString::fromWCharArray(kProgId) +
                        QStringLiteral("\\shell\\open\\command");
    return keyExists(HKEY_CURRENT_USER, baseKey(RegistryScope::User) + cmd) ||
           keyExists(HKEY_LOCAL_MACHINE, baseKey(RegistryScope::Machine) + cmd);
}

bool FileAssociations::isDefaultFor(const QString& extension, const QString& viewerExe)
{
    wchar_t buf[MAX_PATH * 2] = {};
    DWORD len = DWORD(std::size(buf));
    const HRESULT hr = AssocQueryStringW(ASSOCF_NOTRUNCATE | ASSOCF_INIT_IGNOREUNKNOWN, ASSOCSTR_EXECUTABLE,
                                         w(extension).c_str(), L"open", buf, &len);
    if (FAILED(hr))
        return false;
    const QString exe = QString::fromWCharArray(buf);
    const QString wanted = viewerExe.isEmpty() ? QString::fromWCharArray(kExeName) : QFileInfo(viewerExe).fileName();
    return QFileInfo(exe).fileName().compare(wanted, Qt::CaseInsensitive) == 0;
}

QList<AssociationStatus> FileAssociations::status(const QString& viewerExe)
{
    QList<AssociationStatus> out;
    for (const QString& ext : extensions())
        out.push_back({ext, isDefaultFor(ext, viewerExe)});
    return out;
}

bool FileAssociations::openDefaultAppsSettings()
{
    const auto r = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open",
                                                           L"ms-settings:defaultapps?registeredAppUser=Picazoo",
                                                           nullptr, nullptr, SW_SHOWNORMAL));
    if (r > 32)
        return true;
    return reinterpret_cast<INT_PTR>(
               ShellExecuteW(nullptr, L"open", L"ms-settings:defaultapps", nullptr, nullptr, SW_SHOWNORMAL)) > 32;
}

bool FileAssociations::openWithDialog(const QString& path)
{
    const std::wstring p = w(QDir::toNativeSeparators(path));
    OPENASINFO info{};
    info.pcszFile = p.c_str();
    info.oaifInFlags = OAIF_EXEC;
    return SUCCEEDED(SHOpenWithDialog(nullptr, &info));
}

} // namespace picazoo::platform
