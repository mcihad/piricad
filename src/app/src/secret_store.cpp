// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/secret_store.hpp"

#include <QByteArray>
#include <QLatin1String>
#include <QtGlobal>

#if KENTOS_HAVE_KEYCHAIN
#if defined(Q_OS_MACOS)
#include <Security/Security.h>
#elif defined(Q_OS_WIN)
// `windows.h` first: `wincred.h` is not self-contained and will not compile
// without the base types, which is the kind of thing a reader should not have to
// rediscover.
#include <windows.h>

#include <wincred.h>
#elif defined(KENTOS_KEYCHAIN_SECRET_SERVICE)
#include <libsecret/secret.h>
#endif
#endif

namespace kentos::app {
namespace {

/// The conventional environment variable for a short `key_ref`: upper case with
/// `_API_KEY` after it, so the shipped `deepseek` profile finds
/// `DEEPSEEK_API_KEY` — the variable DeepSeek's own documentation tells people to
/// export.
///
/// ASCII UPPER CASE, AND ONLY ASCII. CLAUDE.md 5.6 bans `std::toupper` on
/// TURKISH text; an environment variable name is not Turkish text — POSIX names
/// them from the portable character set and a `ı` in one is not a name any shell
/// can export. `core::turkish_upper` would turn `i` into `İ` and produce a
/// variable that cannot exist, which is the opposite of helping. (The same
/// argument `ai/provider.cpp` makes for a host name.)
QString conventional_variable(const QString& key_ref)
{
    QString out;
    out.reserve(key_ref.size() + 8);
    for (const QChar ch : key_ref) {
        if (ch.unicode() > 127) return {}; // not an environment variable name
        const char c = ch.toLatin1();
        if ((c >= 'a' && c <= 'z')) {
            out.append(QChar::fromLatin1(static_cast<char>(c - 'a' + 'A')));
        } else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
            out.append(ch);
        } else if (c == '-' || c == '.' || c == ' ') {
            out.append(QLatin1Char('_'));
        } else {
            return {};
        }
    }
    if (out.isEmpty()) return {};
    return out + QStringLiteral("_API_KEY");
}

/// The value of the environment variable `name`, or nothing when it is unset or
/// empty. Read at the moment of use and never copied anywhere that persists.
std::optional<QString> from_environment(const QString& name)
{
    if (name.isEmpty()) return std::nullopt;
    const QByteArray key = name.toLocal8Bit();
    if (!qEnvironmentVariableIsSet(key.constData())) return std::nullopt;
    const QString value = qEnvironmentVariable(key.constData());
    if (value.isEmpty()) return std::nullopt;
    return value;
}

#if KENTOS_HAVE_KEYCHAIN && defined(KENTOS_KEYCHAIN_SECRET_SERVICE)

/// The schema our entries are stored under in the Secret Service.
///
/// ONE ATTRIBUTE, `kayit`, holding the profile's `key_ref`. libsecret matches an
/// entry by its attributes rather than by a path, so this is what makes a
/// lookup find the key a store wrote; the schema name is the reverse-DNS form
/// the API expects and must not change, because an entry written under one name
/// cannot be found under another.
const SecretSchema* kentos_secret_schema()
{
    static const SecretSchema schema = {
        "org.kentoscad.Anahtar",
        SECRET_SCHEMA_NONE,
        {
            {"kayit", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {"NULL", static_cast<SecretSchemaAttributeType>(0)},
        },
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
    };
    return &schema;
}

#endif

#if KENTOS_HAVE_KEYCHAIN && defined(Q_OS_MACOS)

/// A `CFStringRef` over a `QString`, released by the guard that owns it.
///
/// The Security framework takes Core Foundation types and nothing else, so every
/// call needs two or three of these; a guard is how they get released on every
/// path including the failure ones.
class CfString
{
public:
    /// Copies `text`'s UTF-8 into a new immutable CFString.
    explicit CfString(const QString& text)
    {
        const QByteArray utf8 = text.toUtf8();
        ref_                  = CFStringCreateWithBytes(
            kCFAllocatorDefault, reinterpret_cast<const UInt8*>(utf8.constData()),
            static_cast<CFIndex>(utf8.size()), kCFStringEncodingUTF8, false);
    }

    /// Releases the string.
    ~CfString()
    {
        if (ref_ != nullptr) CFRelease(ref_);
    }

    CfString(const CfString&)            = delete;
    CfString& operator=(const CfString&) = delete;

    /// The borrowed reference, valid while this guard lives.
    CFStringRef get() const noexcept { return ref_; }

private:
    CFStringRef ref_{nullptr};
};

/// The query that identifies one of our entries: a generic password under the
/// `KentOSCad` service, with the profile's `key_ref` as the account.
CFMutableDictionaryRef entry_query(const CfString& service, const CfString& account)
{
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, service.get());
    CFDictionarySetValue(query, kSecAttrAccount, account.get());
    return query;
}

/// What went wrong, in Turkish, with the `OSStatus` for a bug report. NEVER the
/// secret: the failure path is a message path (CLAUDE.md 5.21).
std::string keychain_trouble(const char* what, OSStatus status)
{
    return std::string(what) + " (Anahtar Zinciri hatası " + std::to_string(status) + ").";
}

#endif

} // namespace

std::optional<QString> SecretStore::read(const QString& key_ref) const
{
    // A LOCAL MODEL HAS NO KEY, and that is the common case rather than an edge
    // one: Ollama and llama.cpp need no credential, so their profiles carry no
    // `key_ref` and nothing should be looked up for them.
    if (key_ref.isEmpty()) return std::nullopt;

#if KENTOS_HAVE_KEYCHAIN && defined(Q_OS_MACOS)
    {
        const CfString service(QString::fromUtf8(kService));
        const CfString account(key_ref);
        CFMutableDictionaryRef query = entry_query(service, account);
        CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
        CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

        CFTypeRef found       = nullptr;
        const OSStatus status = SecItemCopyMatching(query, &found);
        CFRelease(query);
        if (status == errSecSuccess && found != nullptr) {
            const CFDataRef data = static_cast<CFDataRef>(found);
            const QByteArray bytes(reinterpret_cast<const char*>(CFDataGetBytePtr(data)),
                                   static_cast<qsizetype>(CFDataGetLength(data)));
            CFRelease(found);
            const QString secret = QString::fromUtf8(bytes);
            if (!secret.isEmpty()) return secret;
        } else if (found != nullptr) {
            CFRelease(found);
        }
        // Any other status — no such entry, a locked keychain, a user who said
        // no — falls through to the environment rather than failing here: the
        // two roads are alternatives, not a chain of precondition checks.
    }
#elif KENTOS_HAVE_KEYCHAIN && defined(Q_OS_WIN)
    {
        const QString target  = QString::fromUtf8(kService) + QLatin1Char(':') + key_ref;
        PCREDENTIALW found    = nullptr;
        const std::wstring wt = target.toStdWString();
        if (CredReadW(wt.c_str(), CRED_TYPE_GENERIC, 0, &found) != FALSE && found != nullptr) {
            const QString secret =
                QString::fromUtf16(reinterpret_cast<const char16_t*>(found->CredentialBlob),
                                   static_cast<qsizetype>(found->CredentialBlobSize / 2));
            CredFree(found);
            if (!secret.isEmpty()) return secret;
        }
    }
#elif KENTOS_HAVE_KEYCHAIN && defined(KENTOS_KEYCHAIN_SECRET_SERVICE)
    {
        GError* error        = nullptr;
        const QByteArray ref = key_ref.toUtf8();
        gchar* value = secret_password_lookup_sync(kentos_secret_schema(), nullptr, &error, "kayit",
                                                   ref.constData(), nullptr);
        if (error != nullptr) g_error_free(error);
        if (value != nullptr) {
            const QString secret = QString::fromUtf8(value);
            secret_password_free(value);
            if (!secret.isEmpty()) return secret;
        }
    }
#endif

    // THE ENVIRONMENT ROAD, on every platform and in every build (see the header).
    if (auto exact = from_environment(key_ref)) return exact;
    return from_environment(conventional_variable(key_ref));
}

core::Status SecretStore::write(const QString& key_ref, const QString& secret)
{
    if (key_ref.isEmpty())
        return core::err(core::ErrorCode::InvalidArgument,
                         "Anahtar kaydının adı boş; profile bir 'anahtar adı' yazın.");
    if (secret.isEmpty()) return erase(key_ref);

#if KENTOS_HAVE_KEYCHAIN && defined(Q_OS_MACOS)
    const CfString service(QString::fromUtf8(kService));
    const CfString account(key_ref);
    const QByteArray utf8 = secret.toUtf8();
    CFDataRef data =
        CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(utf8.constData()),
                     static_cast<CFIndex>(utf8.size()));

    // ADD, AND ON A DUPLICATE UPDATE. There is no "upsert" in the Security
    // framework, and a `SecItemDelete` before every write would lose the entry
    // when the add then failed.
    CFMutableDictionaryRef add = entry_query(service, account);
    CFDictionarySetValue(add, kSecValueData, data);
    OSStatus status = SecItemAdd(add, nullptr);
    CFRelease(add);

    if (status == errSecDuplicateItem) {
        CFMutableDictionaryRef query = entry_query(service, account);
        CFMutableDictionaryRef fresh =
            CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(fresh, kSecValueData, data);
        status = SecItemUpdate(query, fresh);
        CFRelease(query);
        CFRelease(fresh);
    }
    CFRelease(data);

    if (status != errSecSuccess)
        return core::err(core::ErrorCode::IoFailure,
                         keychain_trouble("Anahtar Anahtar Zinciri'ne yazılamadı", status));
    return core::ok();
#elif KENTOS_HAVE_KEYCHAIN && defined(Q_OS_WIN)
    const QString target          = QString::fromUtf8(kService) + QLatin1Char(':') + key_ref;
    std::wstring wt               = target.toStdWString();
    std::wstring ws               = secret.toStdWString();
    CREDENTIALW credential        = {};
    credential.Type               = CRED_TYPE_GENERIC;
    credential.TargetName         = wt.data();
    credential.CredentialBlobSize = static_cast<DWORD>(ws.size() * sizeof(wchar_t));
    credential.CredentialBlob     = reinterpret_cast<LPBYTE>(ws.data());
    credential.Persist            = CRED_PERSIST_LOCAL_MACHINE;
    if (CredWriteW(&credential, 0) == FALSE)
        return core::err(core::ErrorCode::IoFailure,
                         "Anahtar Windows kimlik deposuna yazılamadı (hata " +
                             std::to_string(GetLastError()) + ").");
    return core::ok();
#elif KENTOS_HAVE_KEYCHAIN && defined(KENTOS_KEYCHAIN_SECRET_SERVICE)
    GError* error         = nullptr;
    const QByteArray ref  = key_ref.toUtf8();
    const QByteArray utf8 = secret.toUtf8();
    const QByteArray label =
        (QString::fromUtf8(kService) + QStringLiteral(" — ") + key_ref).toUtf8();
    const gboolean ok = secret_password_store_sync(
        kentos_secret_schema(), SECRET_COLLECTION_DEFAULT, label.constData(), utf8.constData(),
        nullptr, &error, "kayit", ref.constData(), nullptr);
    if (error != nullptr) {
        const std::string why = error->message != nullptr ? error->message : "bilinmeyen sebep";
        g_error_free(error);
        return core::err(core::ErrorCode::IoFailure,
                         "Anahtar sistem anahtar kasasına yazılamadı: " + why + ".");
    }
    if (ok == FALSE)
        return core::err(core::ErrorCode::IoFailure, "Anahtar sistem anahtar kasasına yazılamadı.");
    return core::ok();
#else
    // NO STORE, AND IT SAYS SO rather than dropping the key on the floor. The
    // message names the road that IS open, with the variable to export.
    (void)secret;
    const QString variable = conventional_variable(key_ref);
    return core::err(
        core::ErrorCode::Unsupported,
        "Bu yapıda sistem anahtar deposu yok (KENTOS_WITH_KEYCHAIN kapalı), bu "
        "yüzden anahtar kaydedilemez. Anahtarı bir ortam değişkeninde tutun: " +
            key_ref.toStdString() +
            (variable.isEmpty() ? std::string() : (" ya da " + variable.toStdString())) +
            " değişkenini ayarlayıp programı yeniden başlatın.");
#endif
}

core::Status SecretStore::erase(const QString& key_ref)
{
    if (key_ref.isEmpty()) return core::ok();

#if KENTOS_HAVE_KEYCHAIN && defined(Q_OS_MACOS)
    const CfString service(QString::fromUtf8(kService));
    const CfString account(key_ref);
    CFMutableDictionaryRef query = entry_query(service, account);
    const OSStatus status        = SecItemDelete(query);
    CFRelease(query);
    if (status != errSecSuccess && status != errSecItemNotFound)
        return core::err(core::ErrorCode::IoFailure,
                         keychain_trouble("Anahtar Anahtar Zinciri'nden silinemedi", status));
    return core::ok();
#elif KENTOS_HAVE_KEYCHAIN && defined(Q_OS_WIN)
    const QString target  = QString::fromUtf8(kService) + QLatin1Char(':') + key_ref;
    const std::wstring wt = target.toStdWString();
    if (CredDeleteW(wt.c_str(), CRED_TYPE_GENERIC, 0) == FALSE && GetLastError() != ERROR_NOT_FOUND)
        return core::err(core::ErrorCode::IoFailure,
                         "Anahtar Windows kimlik deposundan silinemedi (hata " +
                             std::to_string(GetLastError()) + ").");
    return core::ok();
#elif KENTOS_HAVE_KEYCHAIN && defined(KENTOS_KEYCHAIN_SECRET_SERVICE)
    GError* error        = nullptr;
    const QByteArray ref = key_ref.toUtf8();
    secret_password_clear_sync(kentos_secret_schema(), nullptr, &error, "kayit", ref.constData(),
                               nullptr);
    if (error != nullptr) {
        const std::string why = error->message != nullptr ? error->message : "bilinmeyen sebep";
        g_error_free(error);
        return core::err(core::ErrorCode::IoFailure,
                         "Anahtar sistem anahtar kasasından silinemedi: " + why + ".");
    }
    return core::ok();
#else
    // Nothing was ever stored here, so there is nothing to remove and no reason
    // to report a failure: the state the caller asked for already holds.
    return core::ok();
#endif
}

bool SecretStore::available() noexcept
{
#if KENTOS_HAVE_KEYCHAIN &&                                                                        \
    (defined(Q_OS_MACOS) || defined(Q_OS_WIN) || defined(KENTOS_KEYCHAIN_SECRET_SERVICE))
    return true;
#else
    return false;
#endif
}

QString SecretStore::describe()
{
#if KENTOS_HAVE_KEYCHAIN && defined(Q_OS_MACOS)
    return QStringLiteral("macOS Anahtar Zinciri (servis: %1). Anahtar adı bir ortam "
                          "değişkenini de adlandırabilir; o değişken ilk kullanımda okunur "
                          "ve hiçbir yere yazılmaz.")
        .arg(QString::fromUtf8(kService));
#elif KENTOS_HAVE_KEYCHAIN && defined(Q_OS_WIN)
    return QStringLiteral("Windows kimlik deposu (hedef: %1:<anahtar adı>). Anahtar adı bir "
                          "ortam değişkenini de adlandırabilir; o değişken ilk kullanımda "
                          "okunur ve hiçbir yere yazılmaz.")
        .arg(QString::fromUtf8(kService));
#elif KENTOS_HAVE_KEYCHAIN && defined(KENTOS_KEYCHAIN_SECRET_SERVICE)
    return QStringLiteral("Sistem anahtar kasası (libsecret / Secret Service). Anahtar adı bir "
                          "ortam değişkenini de adlandırabilir; o değişken ilk kullanımda "
                          "okunur ve hiçbir yere yazılmaz.");
#else
    return QStringLiteral("Bu yapıda sistem anahtar deposu yok: anahtar kaydedilemez. Anahtar "
                          "adı bir ortam değişkenini adlandırır (örnek: DEEPSEEK_API_KEY) ve o "
                          "değişken ilk kullanımda okunur.");
#endif
}

} // namespace kentos::app
