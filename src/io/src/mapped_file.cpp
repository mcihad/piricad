// SPDX-License-Identifier: GPL-3.0-or-later
#include "mapped_file.hpp"

#include <cerrno>
#include <cstring>
#include <string>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace kentos::io {
namespace {

using core::ErrorCode;

/// The operating system's own words, so a permission problem does not arrive as
/// "açılamadı" and nothing else (core.md R10: actionable messages only).
std::string system_reason()
{
#ifdef _WIN32
    const DWORD code = ::GetLastError();
    char* text       = nullptr;
    const DWORD n = ::FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                         FORMAT_MESSAGE_IGNORE_INSERTS,
                                     nullptr, code, 0, reinterpret_cast<char*>(&text), 0, nullptr);
    std::string out = n && text ? std::string(text, n) : ("hata kodu " + std::to_string(code));
    if (text) ::LocalFree(text);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
    return out;
#else
    return std::string(std::strerror(errno));
#endif
}

} // namespace

MappedFile::~MappedFile()
{
    close();
}

MappedFile::MappedFile(MappedFile&& o) noexcept
    : data_(std::exchange(o.data_, nullptr)), size_(std::exchange(o.size_, 0)),
      handle_(std::exchange(o.handle_, -1)), mapping_(std::exchange(o.mapping_, -1))
{}

MappedFile& MappedFile::operator=(MappedFile&& o) noexcept
{
    if (this != &o) {
        close();
        data_    = std::exchange(o.data_, nullptr);
        size_    = std::exchange(o.size_, 0);
        handle_  = std::exchange(o.handle_, -1);
        mapping_ = std::exchange(o.mapping_, -1);
    }
    return *this;
}

void MappedFile::close() noexcept
{
#ifdef _WIN32
    if (data_) ::UnmapViewOfFile(data_);
    if (mapping_ != -1) ::CloseHandle(reinterpret_cast<HANDLE>(mapping_));
    if (handle_ != -1) ::CloseHandle(reinterpret_cast<HANDLE>(handle_));
#else
    if (data_) ::munmap(const_cast<void*>(data_), size_);
    if (handle_ != -1) ::close(static_cast<int>(handle_));
#endif
    data_    = nullptr;
    size_    = 0;
    handle_  = -1;
    mapping_ = -1;
}

#ifdef _WIN32

core::Result<MappedFile> MappedFile::open(const std::string& path)
{
    MappedFile m;

    const HANDLE file =
        ::CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return core::err(ErrorCode::IoFailure, "'" + path + "' açılamadı: " + system_reason() +
                                                   ". Yolu ve okuma iznini denetleyin.");
    m.handle_ = reinterpret_cast<std::intptr_t>(file);

    LARGE_INTEGER size{};
    if (!::GetFileSizeEx(file, &size))
        return core::err(ErrorCode::IoFailure,
                         "'" + path + "' boyutu okunamadı: " + system_reason());
    if (size.QuadPart <= 0)
        return core::err(ErrorCode::ParseError,
                         "'" + path + "' boş; KentOSCad proje dosyası değil.");
    m.size_ = static_cast<std::size_t>(size.QuadPart);

    const HANDLE mapping = ::CreateFileMappingA(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping)
        return core::err(ErrorCode::IoFailure,
                         "'" + path + "' belleğe eşlenemedi: " + system_reason());
    m.mapping_ = reinterpret_cast<std::intptr_t>(mapping);

    const void* view = ::MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (!view)
        return core::err(ErrorCode::IoFailure,
                         "'" + path + "' görünümü açılamadı: " + system_reason());
    m.data_ = view;

    return m;
}

#else

core::Result<MappedFile> MappedFile::open(const std::string& path)
{
    MappedFile m;

    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return core::err(ErrorCode::IoFailure, "'" + path + "' açılamadı: " + system_reason() +
                                                   ". Yolu ve okuma iznini denetleyin.");
    m.handle_ = fd;

    struct stat st{};
    if (::fstat(fd, &st) != 0)
        return core::err(ErrorCode::IoFailure,
                         "'" + path + "' boyutu okunamadı: " + system_reason());
    if (!S_ISREG(st.st_mode))
        return core::err(ErrorCode::IoFailure,
                         "'" + path + "' sıradan bir dosya değil; proje dosyası bekleniyordu.");
    if (st.st_size <= 0)
        return core::err(ErrorCode::ParseError,
                         "'" + path + "' boş; KentOSCad proje dosyası değil.");
    m.size_ = static_cast<std::size_t>(st.st_size);

    void* view = ::mmap(nullptr, m.size_, PROT_READ, MAP_PRIVATE, fd, 0);
    if (view == MAP_FAILED) {
        m.size_ = 0;
        return core::err(ErrorCode::IoFailure,
                         "'" + path + "' belleğe eşlenemedi: " + system_reason());
    }
    m.data_ = view;

    return m;
}

#endif

} // namespace kentos::io
