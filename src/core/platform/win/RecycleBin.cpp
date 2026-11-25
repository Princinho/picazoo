#include "platform/win/RecycleBin.h"

#include "platform/win/Paths.h"

#include <windows.h>

#include <sddl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QFile>

#include <algorithm>
#include <optional>
#include <string>

namespace picazoo::platform {

namespace {

// Initialise COM (STA) pour la durée d'un appel.
struct ComScope {
    HRESULT hr;
    ComScope() : hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)) {}
    ~ComScope()
    {
        if (SUCCEEDED(hr))
            CoUninitialize();
    }
};

template <typename T> struct ComPtr {
    T* p = nullptr;
    ~ComPtr()
    {
        if (p)
            p->Release();
    }
    T** put() { return &p; }
    T* operator->() const { return p; }
    explicit operator bool() const { return p != nullptr; }
};

std::wstring volumeRoot(const QString& path)
{
    wchar_t buf[MAX_PATH + 1] = {};
    if (!GetVolumePathNameW(toWin32(path).c_str(), buf, MAX_PATH))
        return {};
    std::wstring root(buf);
    // GetVolumePathNameW peut renvoyer le préfixe \\?\ : on le retire pour les API shell.
    if (root.rfind(L"\\\\?\\", 0) == 0 && root.size() > 5 && root[5] == L':')
        root = root.substr(4);
    return root;
}

bool pathExists(const QString& path)
{
    return GetFileAttributesW(toWin32(path).c_str()) != INVALID_FILE_ATTRIBUTES;
}

qint64 fileTimeToMs(qint64 ft)
{
    // FILETIME : centaines de ns depuis 1601 ; epoch Unix : 11644473600 s plus tard.
    return ft / 10000 - 11644473600000LL;
}

QString currentUserSid()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return {};
    DWORD len = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &len);
    std::string buf(len, '\0');
    QString out;
    if (len && GetTokenInformation(token, TokenUser, buf.data(), len, &len)) {
        auto* user = reinterpret_cast<TOKEN_USER*>(buf.data());
        LPWSTR str = nullptr;
        if (ConvertSidToStringSidW(user->User.Sid, &str)) {
            out = QString::fromWCharArray(str);
            LocalFree(str);
        }
    }
    CloseHandle(token);
    return out;
}

// Puits de progression : refuse tout élément que le système n'enverrait pas à la corbeille.
class RecycleOnlySink : public IFileOperationProgressSink {
public:
    bool refused = false;
    bool sawPost = false;
    HRESULT deleteResult = S_OK;

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (riid == IID_IUnknown || riid == __uuidof(IFileOperationProgressSink)) {
            *ppv = static_cast<IFileOperationProgressSink*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return ++refs_; }
    IFACEMETHODIMP_(ULONG) Release() override { return --refs_; } // vit sur la pile de l'appelant

    IFACEMETHODIMP StartOperations() override { return S_OK; }
    IFACEMETHODIMP FinishOperations(HRESULT) override { return S_OK; }
    IFACEMETHODIMP PreRenameItem(DWORD, IShellItem*, LPCWSTR) override { return S_OK; }
    IFACEMETHODIMP PostRenameItem(DWORD, IShellItem*, LPCWSTR, HRESULT, IShellItem*) override { return S_OK; }
    IFACEMETHODIMP PreMoveItem(DWORD, IShellItem*, IShellItem*, LPCWSTR) override { return S_OK; }
    IFACEMETHODIMP PostMoveItem(DWORD, IShellItem*, IShellItem*, LPCWSTR, HRESULT, IShellItem*) override { return S_OK; }
    IFACEMETHODIMP PreCopyItem(DWORD, IShellItem*, IShellItem*, LPCWSTR) override { return S_OK; }
    IFACEMETHODIMP PostCopyItem(DWORD, IShellItem*, IShellItem*, LPCWSTR, HRESULT, IShellItem*) override { return S_OK; }
    IFACEMETHODIMP PreDeleteItem(DWORD flags, IShellItem*) override
    {
        // Verrou de sécurité (research D8) : sans ce drapeau, le système s'apprête à effacer
        // définitivement. On refuse : le fichier reste en place.
        if (!(flags & TSF_DELETE_RECYCLE_IF_POSSIBLE)) {
            refused = true;
            return E_ABORT;
        }
        return S_OK;
    }
    IFACEMETHODIMP PostDeleteItem(DWORD, IShellItem*, HRESULT hr, IShellItem*) override
    {
        sawPost = true;
        deleteResult = hr;
        return S_OK;
    }
    IFACEMETHODIMP PreNewItem(DWORD, IShellItem*, LPCWSTR) override { return S_OK; }
    IFACEMETHODIMP PostNewItem(DWORD, IShellItem*, LPCWSTR, LPCWSTR, DWORD, HRESULT, IShellItem*) override { return S_OK; }
    IFACEMETHODIMP UpdateProgress(UINT, UINT) override { return S_OK; }
    IFACEMETHODIMP ResetTimer() override { return S_OK; }
    IFACEMETHODIMP PauseTimer() override { return S_OK; }
    IFACEMETHODIMP ResumeTimer() override { return S_OK; }

private:
    ULONG refs_ = 1;
};

bool isLockedOrReadOnly(const QString& path)
{
    const std::wstring w = toWin32(path);
    const DWORD attrs = GetFileAttributesW(w.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY))
        return true;
    // Ouvrir avec le droit DELETE échoue si un autre programme tient le fichier sans partage.
    HANDLE h = CreateFileW(w.c_str(), DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        const DWORD err = GetLastError();
        return err == ERROR_SHARING_VIOLATION || err == ERROR_ACCESS_DENIED || err == ERROR_LOCK_VIOLATION;
    }
    CloseHandle(h);
    return false;
}

} // namespace

bool volumeHasRecycleBin(const QString& path)
{
    const std::wstring root = volumeRoot(path);
    if (root.empty() || GetDriveTypeW(root.c_str()) != DRIVE_FIXED)
        return false;
    ComScope com;
    SHQUERYRBINFO info{};
    info.cbSize = sizeof(info);
    return SUCCEEDED(SHQueryRecycleBinW(root.c_str(), &info));
}

bool volumeHasRecycleBinCached(const QString& path)
{
    static QMutex mutex;
    static QHash<QString, bool> cache;
    const QString root = QString::fromStdWString(volumeRoot(path)).toUpper();
    {
        QMutexLocker lock(&mutex);
        const auto it = cache.constFind(root);
        if (it != cache.constEnd())
            return *it;
    }
    const bool has = volumeHasRecycleBin(path);
    QMutexLocker lock(&mutex);
    cache.insert(root, has);
    return has;
}

RecycleResult moveToRecycleBin(const QString& path, QString* message)
{
    auto fail = [&](RecycleResult r, const QString& m) {
        if (message)
            *message = m;
        return r;
    };
    if (!pathExists(path))
        return fail(RecycleResult::Missing, QStringLiteral("Fichier introuvable."));
    if (!volumeHasRecycleBin(path))
        return fail(RecycleResult::NoRecycleBin, QStringLiteral("Ce disque n'a pas de corbeille."));
    if (isLockedOrReadOnly(path))
        return fail(RecycleResult::Locked,
                    QStringLiteral("Fichier utilisé par un autre programme ou protégé en écriture."));

    ComScope com;
    ComPtr<IFileOperation> op;
    if (FAILED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(op.put()))))
        return fail(RecycleResult::Failed, QStringLiteral("Service de fichiers de Windows indisponible."));
    op->SetOperationFlags(FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI |
                          FOFX_RECYCLEONDELETE | FOFX_EARLYFAILURE);
    ComPtr<IShellItem> item;
    const std::wstring w = path.toStdWString();
    if (FAILED(SHCreateItemFromParsingName(w.c_str(), nullptr, IID_PPV_ARGS(item.put()))))
        return fail(RecycleResult::Failed, QStringLiteral("Chemin non reconnu par Windows."));
    RecycleOnlySink sink;
    if (FAILED(op->DeleteItem(item.p, &sink)))
        return fail(RecycleResult::Failed, QStringLiteral("Opération refusée par Windows."));
    const HRESULT hr = op->PerformOperations();

    if (sink.refused) {
        if (pathExists(path))
            return fail(RecycleResult::NoRecycleBin,
                        QStringLiteral("Windows l'aurait supprimé définitivement : fichier laissé en place."));
        // Ne doit pas arriver : refus dans PreDeleteItem mais fichier disparu.
        return fail(RecycleResult::Failed, QStringLiteral("Résultat inattendu de Windows."));
    }
    if (pathExists(path)) {
        const HRESULT d = sink.sawPost ? sink.deleteResult : hr;
        if (d == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION) || d == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED) ||
            d == E_ACCESSDENIED)
            return fail(RecycleResult::Locked, QStringLiteral("Fichier utilisé par un autre programme."));
        return fail(RecycleResult::Failed,
                    QStringLiteral("Windows n'a pas pu le mettre à la corbeille (0x%1).").arg(quint32(d), 8, 16, QLatin1Char('0')));
    }
    if (message)
        message->clear();
    return RecycleResult::Recycled;
}

QList<RecycledEntry> recycledEntries(const QString& anyPathOnVolume)
{
    QList<RecycledEntry> out;
    const std::wstring root = volumeRoot(anyPathOnVolume);
    const QString sid = currentUserSid();
    if (root.empty() || sid.isEmpty())
        return out;
    const QString dir = QString::fromStdWString(root) + QStringLiteral("$Recycle.Bin\\") + sid + u'\\';
    WIN32_FIND_DATAW fd{};
    HANDLE find = FindFirstFileW((dir + QStringLiteral("$I*")).toStdWString().c_str(), &fd);
    if (find == INVALID_HANDLE_VALUE)
        return out;
    do {
        const QString name = QString::fromWCharArray(fd.cFileName);
        QFile f(dir + name);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QByteArray b = f.readAll();
        if (b.size() < 24)
            continue;
        qint64 version = 0, size = 0, ft = 0;
        memcpy(&version, b.constData(), 8);
        memcpy(&size, b.constData() + 8, 8);
        memcpy(&ft, b.constData() + 16, 8);
        QString original;
        if (version == 2 && b.size() >= 28) {
            qint32 len = 0;
            memcpy(&len, b.constData() + 24, 4);
            if (len > 0 && 28 + qsizetype(len) * 2 <= b.size())
                original = QString::fromUtf16(reinterpret_cast<const char16_t*>(b.constData() + 28), len).section(QChar(0), 0, 0);
        } else if (version == 1 && b.size() >= 24 + 520) {
            original = QString::fromUtf16(reinterpret_cast<const char16_t*>(b.constData() + 24), 260).section(QChar(0), 0, 0);
        }
        if (original.isEmpty())
            continue;
        RecycledEntry e;
        e.originalPath = normalizePath(original);
        e.infoPath = dir + name;
        e.recycledPath = dir + QStringLiteral("$R") + name.mid(2);
        e.deletedAtMs = fileTimeToMs(ft);
        e.sizeBytes = size;
        out << e;
    } while (FindNextFileW(find, &fd));
    FindClose(find);
    return out;
}

RestoreResult restoreFromRecycleBin(const QString& originalPath, qint64 deletedAfterMs, QString* message)
{
    auto fail = [&](RestoreResult r, const QString& m) {
        if (message)
            *message = m;
        return r;
    };
    const QString target = normalizePath(originalPath);
    std::optional<RecycledEntry> best;
    for (const RecycledEntry& e : recycledEntries(target)) {
        if (!samePath(e.originalPath, target) || e.deletedAtMs < deletedAfterMs - 5000)
            continue;
        if (!best || e.deletedAtMs > best->deletedAtMs)
            best = e;
    }
    if (!best || !pathExists(best->recycledPath))
        return fail(RestoreResult::NotInRecycleBin, QStringLiteral("Ce fichier n'est plus dans la corbeille."));
    if (pathExists(target))
        return fail(RestoreResult::Conflict, QStringLiteral("Un fichier du même nom existe déjà à cet emplacement."));

    ComScope com;
    // L'élément correspondant dans le dossier shell « Corbeille », pour le restaurer comme
    // l'Explorateur (verbe « undelete »), ce qui met aussi à jour la corbeille.
    ComPtr<IShellItem> bin;
    if (FAILED(SHGetKnownFolderItem(FOLDERID_RecycleBinFolder, KF_FLAG_DEFAULT, nullptr, IID_PPV_ARGS(bin.put()))))
        return fail(RestoreResult::Failed, QStringLiteral("Corbeille inaccessible."));
    ComPtr<IEnumShellItems> items;
    if (FAILED(bin->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(items.put()))))
        return fail(RestoreResult::Failed, QStringLiteral("Corbeille inaccessible."));
    IShellItem* found = nullptr;
    for (;;) {
        IShellItem* it = nullptr;
        if (items->Next(1, &it, nullptr) != S_OK)
            break;
        LPWSTR fs = nullptr;
        bool match = false;
        if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &fs))) {
            match = samePath(normalizePath(QString::fromWCharArray(fs)), normalizePath(best->recycledPath));
            CoTaskMemFree(fs);
        }
        if (match) {
            found = it;
            break;
        }
        it->Release();
    }
    if (!found)
        return fail(RestoreResult::NotInRecycleBin, QStringLiteral("Ce fichier n'est plus dans la corbeille."));

    bool restored = false;
    IContextMenu* menu = nullptr;
    if (SUCCEEDED(found->BindToHandler(nullptr, BHID_SFUIObject, IID_PPV_ARGS(&menu)))) {
        HMENU h = CreatePopupMenu();
        if (h && SUCCEEDED(menu->QueryContextMenu(h, 0, 1, 0x7FFF, CMF_NORMAL))) {
            CMINVOKECOMMANDINFOEX ci{};
            ci.cbSize = sizeof(ci);
            ci.fMask = CMIC_MASK_FLAG_NO_UI | CMIC_MASK_NOASYNC | CMIC_MASK_UNICODE;
            ci.lpVerb = "undelete";
            ci.lpVerbW = L"undelete";
            ci.nShow = SW_HIDE;
            restored = SUCCEEDED(menu->InvokeCommand(reinterpret_cast<CMINVOKECOMMANDINFO*>(&ci)));
        }
        if (h)
            DestroyMenu(h);
        menu->Release();
    }
    found->Release();
    if (!restored || !pathExists(target))
        return fail(RestoreResult::Failed, QStringLiteral("Windows n'a pas pu restaurer ce fichier."));
    if (message)
        message->clear();
    return RestoreResult::Restored;
}

} // namespace picazoo::platform
