// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/data_root.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>

#include <array>

namespace piricad::app {
namespace {

/// True when `dir` looks like the shipped data tree rather than any directory
/// that happens to be called `data`.
bool holds_catalogues(const QString& dir)
{
    return !dir.isEmpty() && QFileInfo(dir + QStringLiteral("/catalogs")).isDir();
}

} // namespace

std::string data_root()
{
    // Resolved ONCE. The answer cannot change while the process runs, and every
    // caller of `data_path` would otherwise stat the same four directories.
    static const std::string root = [] {
        const QString exe = QCoreApplication::applicationDirPath();

        const std::array<QString, 4> candidates{
            qEnvironmentVariable("PIRICAD_DATA"),
            exe.isEmpty() ? QString() : exe + QStringLiteral("/../share/piricad/data"),
            exe.isEmpty() ? QString() : exe + QStringLiteral("/data"),
            QStringLiteral(PIRICAD_SOURCE_DATA_DIR),
        };

        for (const QString& candidate : candidates) {
            if (!holds_catalogues(candidate)) continue;
            return QDir(candidate).absolutePath().toStdString();
        }
        return std::string{};
    }();
    return root;
}

std::string data_path(const std::string& relative)
{
    if (relative.empty()) return relative;

    const QString given = QString::fromStdString(relative);
    if (QFileInfo(given).isAbsolute()) return relative;

    const std::string root = data_root();
    if (root.empty()) return relative;

    // The setting names its path from the REPOSITORY root — `data/catalogs/...` —
    // because that is what a developer types and what the documentation prints.
    // `data_root()` already points at that `data`, so the prefix is dropped rather
    // than doubled.
    QString tail = given;
    if (tail.startsWith(QStringLiteral("data/"))) tail = tail.mid(5);

    return QDir(QString::fromStdString(root)).filePath(tail).toStdString();
}

} // namespace piricad::app
