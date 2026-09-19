// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/io/pdf_encrypt.hpp"

#if KENTOS_HAVE_QPDF
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFWriter.hh>

#include <exception>
#endif

namespace kentos::io {

bool pdf_encryption_available() noexcept
{
#if KENTOS_HAVE_QPDF
    return true;
#else
    return false;
#endif
}

#if KENTOS_HAVE_QPDF

core::Status pdf_encrypt(const std::string& in, const std::string& out,
                         const PdfEncryption& options)
{
    if (in == out)
        return core::err(core::ErrorCode::InvalidArgument,
                         "PDF şifreleme kaynağı ve hedefi aynı dosya olamaz.");
    try {
        QPDF pdf;
        pdf.processFile(in.c_str());
        if (!options.author.empty()) {
            QPDFObjectHandle trailer = pdf.getTrailer();
            QPDFObjectHandle info    = trailer.getKey("/Info");
            if (!info.isDictionary()) {
                info = pdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
                trailer.replaceKey("/Info", info);
            }
            info.replaceKey("/Author", QPDFObjectHandle::newUnicodeString(options.author));
        }
        QPDFWriter writer(pdf, out.c_str());
        if (options.encrypts()) {
            // AES-256 (R6), the only password scheme the specification endorses.
            // The owner password falls back to the user's so the file always has
            // one that lifts the permissions; a PDF with an empty owner password
            // is one every reader may edit.
            const std::string owner =
                options.owner_password.empty() ? options.user_password : options.owner_password;
            writer.setR6EncryptionParameters(options.user_password.c_str(), owner.c_str(),
                                             /*allow_accessibility=*/true, options.allow_copy,
                                             /*allow_assemble=*/options.allow_modify,
                                             /*allow_annotate_and_form=*/options.allow_modify,
                                             /*allow_form_filling=*/options.allow_modify,
                                             /*allow_modify_other=*/options.allow_modify,
                                             options.allow_print ? qpdf_r3p_full : qpdf_r3p_none,
                                             /*encrypt_metadata_aes=*/true);
        }
        writer.write();
    } catch (const std::exception& e) {
        return core::err(core::ErrorCode::IoFailure, std::string("PDF şifrelenemedi: ") + e.what());
    }
    return core::ok();
}

core::Result<bool> pdf_is_encrypted(const std::string& path)
{
    try {
        QPDF pdf;
        pdf.processFile(path.c_str());
        return pdf.isEncrypted();
    } catch (const QPDFExc& e) {
        // A FILE THAT REFUSES AN EMPTY PASSWORD IS ENCRYPTED, which is the whole
        // question this function is asked. Opening it without one throws, and
        // reporting that as "could not be read" would make every password-
        // protected PDF look like a corrupt one.
        if (e.getErrorCode() == qpdf_e_password) return true;
        return core::err(core::ErrorCode::IoFailure, std::string("PDF okunamadı: ") + e.what());
    } catch (const std::exception& e) {
        return core::err(core::ErrorCode::IoFailure, std::string("PDF okunamadı: ") + e.what());
    }
}

#else

core::Status pdf_encrypt(const std::string&, const std::string&, const PdfEncryption&)
{
    return core::err(core::ErrorCode::Unsupported,
                     "Bu yapı PDF şifrelemeyi içermiyor (KENTOS_WITH_QPDF kapalı ya da qpdf "
                     "bulunamadı). PDF şifresiz yazılabilir.");
}

core::Result<bool> pdf_is_encrypted(const std::string&)
{
    return core::err(core::ErrorCode::Unsupported,
                     "Bu yapı PDF şifrelemeyi içermiyor (KENTOS_WITH_QPDF).");
}

#endif

} // namespace kentos::io
