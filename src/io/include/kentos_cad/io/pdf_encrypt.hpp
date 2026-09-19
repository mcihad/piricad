// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: PDF encryption.
//
// The PDF a plot produces is written by Qt (`QPdfWriter`), which knows nothing
// of passwords. Encrypting it — AES-256, the one password scheme the PDF
// specification still endorses — is qpdf's job (Apache-2.0, CLAUDE.md 2.7): it
// reads the finished file and writes it back encrypted with the permissions
// asked for. Behind `KENTOS_WITH_QPDF`; without it the function says so and
// the print dialog's password fields say so too.
#pragma once

#include "kentos_cad/core/result.hpp"

#include <string>

namespace kentos::io {

/// What the reader may do, with which passwords, and who wrote it.
struct PdfEncryption
{
    std::string user_password;  ///< needed to open the file; empty = anyone opens it
    std::string owner_password; ///< needed to lift the permissions; empty = same as user
    bool allow_print{true};     ///< print the document at all
    bool allow_copy{true};      ///< extract text and graphics
    bool allow_modify{true};    ///< change the document, annotate, fill forms

    /// The document's `/Author`, written in the same pass. Qt's writer has a
    /// title and a creator but no author field, so it is set here.
    std::string author;

    /// Whether any password was given; without one the pass only sets the author.
    bool encrypts() const noexcept { return !user_password.empty() || !owner_password.empty(); }
};

/// Whether this build can encrypt at all.
bool pdf_encryption_available() noexcept;

/// Reads the PDF at `in`, writes it to `out` encrypted (when a password is
/// given) and with its author set (when one is given). `in` and `out` may not be
/// the same path: qpdf reads lazily while it writes.
core::Status pdf_encrypt(const std::string& in, const std::string& out,
                         const PdfEncryption& options);

/// Whether the PDF at `path` is encrypted, for a test and for the print dialog's
/// report. An error when the file is not a PDF this build can read.
core::Result<bool> pdf_is_encrypted(const std::string& path);

} // namespace kentos::io
