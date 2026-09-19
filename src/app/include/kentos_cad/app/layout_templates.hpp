// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the office's standard paftas.
//
// A TEMPLATE IS A PAFTA THAT BELONGS TO NOBODY'S DRAWING. The firm's sheet — its
// title block, its legend box, its grid settings — is used on every job, so it
// cannot live in one job's file. It lives in the user's configuration directory
// as a folder of JSON documents, one per template, which is the form a person
// can copy to a colleague, put in version control and patch when the title block
// changes (`core/layout.hpp` says why JSON rather than the document's blocks).
//
// ONE FILE PER TEMPLATE, not one file holding all of them, and that is the one
// place this store departs from `PrintProfiles`. A template is a document in its
// own right: it is mailed, diffed and replaced individually, and a single
// `pafta-sablonlari.json` would make "send me your ada paftası" mean "send me
// your whole library".
//
// THIS SERVICE NEVER BUILDS A LAYOUT. It reads and writes bytes; `/src/core`
// parses them and `/src/command` writes the result through a transaction, so a
// template applied is an ordinary undoable edit (Article 1.1).
#pragma once

#include "kentos_cad/command/bus.hpp"

#include <QObject>
#include <QString>

namespace kentos::app {

/// The template folder, and the `PAFTAŞABLON` verbs over it.
class LayoutTemplates : public QObject
{
    Q_OBJECT

public:
    /// Installs `Bus::on_layout_template_request` and clears it on destruction.
    explicit LayoutTemplates(command::Bus& bus, QObject* parent = nullptr);
    ~LayoutTemplates() override;

    LayoutTemplates(const LayoutTemplates&)            = delete;
    LayoutTemplates& operator=(const LayoutTemplates&) = delete;

    /// Where the templates live, for the settings page's note and the manager.
    const QString& folder() const noexcept { return folder_; }

    /// The names the folder holds, sorted. Read by the menu and the manager.
    QStringList names() const;

signals:
    /// A template was saved or removed; the menu rebuilds from `names()`.
    void changed();

private:
    command::Task<core::Result<std::string>> handle(command::LayoutTemplateRequest request);

    /// The file a template of that name lives in. Refuses a name that would
    /// escape the folder — a template called `../../etc/passwd` is a name, not a
    /// path, and treating it as one is how a settings folder becomes a weapon.
    core::Result<QString> pathFor(const QString& name) const;

    command::Bus& bus_;
    QString folder_;
};

} // namespace kentos::app
