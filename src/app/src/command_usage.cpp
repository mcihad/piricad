// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/command_usage.hpp"

#include "piricad/command/registry.hpp"

#include <QSettings>
#include <QStringList>

#include <algorithm>

namespace piricad::app {
namespace {

constexpr const char* kRecentKey    = "ui/komut_son";
constexpr const char* kFavouriteKey = "ui/komut_favori";

QStringList to_qt(const std::vector<std::string>& ids)
{
    QStringList out;
    for (const std::string& id : ids)
        out << QString::fromStdString(id);
    return out;
}

std::vector<std::string> from_qt(const QStringList& list)
{
    std::vector<std::string> out;
    for (const QString& id : list)
        if (!id.isEmpty() && std::ranges::find(out, id.toStdString()) == out.end())
            out.push_back(id.toStdString());
    return out;
}

} // namespace

CommandUsage::CommandUsage(bool persistent, QObject* parent)
    : QObject(parent), persistent_(persistent)
{
    if (persistent_) load();
}

bool CommandUsage::isFavourite(const std::string& id) const
{
    return std::ranges::find(favourite_, id) != favourite_.end();
}

void CommandUsage::note(const std::string& id)
{
    // Already first: nothing moved, nothing to write — and the palette is not asked
    // to redraw for a command that was run twice in a row.
    if (!recent_.empty() && recent_.front() == id) return;

    command::note_use(recent_, id, kRecentLimit);
    save();
    emit changed();
}

bool CommandUsage::toggleFavourite(const std::string& id)
{
    const bool starred = command::toggle_member(favourite_, id);
    save();
    emit changed();
    return starred;
}

void CommandUsage::load()
{
    const QSettings file;
    recent_ = from_qt(file.value(kRecentKey).toStringList());
    if (recent_.size() > kRecentLimit) recent_.resize(kRecentLimit);
    favourite_ = from_qt(file.value(kFavouriteKey).toStringList());
}

void CommandUsage::save() const
{
    if (!persistent_) return;
    QSettings file;
    file.setValue(kRecentKey, to_qt(recent_));
    file.setValue(kFavouriteKey, to_qt(favourite_));
}

} // namespace piricad::app
