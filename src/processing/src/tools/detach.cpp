// SPDX-License-Identifier: GPL-3.0-or-later
// islem.bag_coz — BAĞÇÖZ: captions set free.
//
// The reverse of BAĞLA and of what UZUNLUKYAZ and KÖŞENUMARALA do as they write:
// the captions in scope stop following anything. They stay where they are and
// say what they say; from here on the object they were about moves alone. A
// caption that follows nothing is skipped and counted, never reported as changed.
#include "kentos_cad/processing/registry.hpp"

namespace kentos::processing {
namespace {

class Detach final : public ProcessingTool
{
public:
    const ToolSpec& spec() const noexcept override { return spec_; }

    core::Status run(const ToolInput& input, ToolOutput& output,
                     const Progress& progress) const override
    {
        std::size_t free_already = 0;
        std::size_t done         = 0;
        for (const InputEntity& e : input.entities) {
            if (progress.cancelled()) return cancelled();
            if (!e.attach) {
                ++free_already;
                progress.at(++done, input.entities.size());
                continue;
            }
            ToolOutput::Replacement r;
            r.key    = e.key;
            r.detach = true;
            output.replacements.push_back(std::move(r));
            ++output.touched;
            progress.at(++done, input.entities.size());
        }
        if (free_already != 0)
            output.notes.push_back(std::to_string(free_already) +
                                   " yazı zaten hiçbir nesneye bağlı değildi.");
        return core::ok();
    }

private:
    const ToolSpec spec_{
        .id     = "islem.bag_coz",
        .python = "detach",
        .names  = {"BAĞÇÖZ", "BAGCOZ", "DETACH", "BÇ", "BC"},
        .title  = "Yazının bağını çöz",
        .summary = "Kapsamdaki yazıların bağını çözer: yazı yerinde kalır, bağlı olduğu nesne "
                   "bundan sonra tek başına taşınır.",
        .group         = "Etiketleme",
        .icon          = "bag_coz",
        .applies       = Applies::Texts,
        .params        = {},
        .output        = OutputShape::InPlace,
        .output_suffix = "",
    };
};

} // namespace

KENTOS_PROCESSING_TOOL(detach)
{
    static const Detach tool;
    return tool;
}

} // namespace kentos::processing
