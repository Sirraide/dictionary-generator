#include <dictgen/backends.hh>

using namespace dict;

struct TypstBackend::Renderer : dict::Renderer {
    TypstBackend& backend;
    const bool raw_text;
    Renderer(TypstBackend& backend, bool strip_formatting)
        : backend(backend), raw_text(strip_formatting) {}

    void render_macro(const MacroNode& n) override;
    void render_text(str text) override;
    void render_formatting(str formatting) override;
};

void TypstBackend::Renderer::render_macro(const MacroNode& n) {
    if (raw_text) return;
    // Use #text rather than ** or __ because it nests properly (#text can reset
    // another #text but not ** o __).
    switch (n.macro) {
        case Macro::Bold: out += "#text(weight: \"bold\")["; break;
        case Macro::Ellipsis: out += "..."; return;
        case Macro::Italic: out += "#text(style: \"italic\")["; break;
        case Macro::Lemma: out += "#lemma["; break;
        case Macro::Normal: out += "#text(style: \"normal\", weight: \"regular\")["; break;
        case Macro::ParagraphBreak: out += "#parbreak()"; return;
        case Macro::Sense: out += "#sense["; break;
        case Macro::SmallCaps: out += "#smallcaps["; break;
        case Macro::Subscript: out += "#sub["; break;
        case Macro::Superscript: out += "#super["; break;
        case Macro::SoftHyphen: out += "-?"; return;
        case Macro::This:
            if (backend.current_word.empty()) backend.error("'\\this' is not allowed here");
            out += backend.current_word;
            return;
    }

    render(n.args);
    out += "]";
}

void TypstBackend::Renderer::render_text(str text) {
    out += text.escape("*_`<@=-+/\\~#$");
}

void TypstBackend::Renderer::render_formatting(str formatting) {
    if (raw_text) return;
    dict::Renderer::render_formatting(formatting);
}

auto TypstBackend::convert(str input, bool strip_macros) -> std::string {
    auto res = TexParser::Parse(*this, input);
    if (not res.has_value()) {
        error("{}", res.error());
        return "";
    }

    Renderer r{*this, strip_macros};
    r.render(*res.value());
    return std::move(r.out);
}

void TypstBackend::emit(str word, const RefEntry& data) {
    output += std::format("#dictionary-reference([{}], [{}])\n", word, data);
}

void TypstBackend::emit(str word, const FullEntry& data) {
    auto FormatSense = [&](const FullEntry::Sense& s) -> std::string {
        if (s.comment.empty() and s.examples.empty() and s.def.empty())
            return "(def: [], comment: [], examples: ())";

        auto sense = std::format(
            "(def: [{}], comment: [{}], examples: (",
            convert(s.def),
            convert(s.comment)
        );

        for (const auto& e : s.examples) {
            sense += std::format(
                "(text: [{}], comment: [{}]),",
                convert(e.text),
                convert(e.comment)
            );
        }

        sense += "))";
        return sense;
    };

    auto ipa = ops.to_ipa(convert(word, false));
    if (not ipa.has_value()) {
        error("Failed to convert '{}' to IPA: {}", word, ipa.error());
        ipa = "ERROR";
    }

    current_word = convert(word);
    output += std::format(
        "#dictionary-entry((word: [{}], pos: [{}], etym: [{}], forms: [{}], ipa: [{}], prim_def: {}, senses: ({})))\n",
        current_word,
        convert(data.pos),
        convert(data.etym),
        convert(data.forms),
        ipa.value(),
        FormatSense(data.primary_definition),
        utils::join(data.senses, "", "{},", FormatSense)
    );
}

void TypstBackend::emit_error(std::string error) {
    errors += error;
    if (not errors.ends_with('\n')) errors += "\n";
}

void TypstBackend::finish() {
    if (has_error) {
        output = "#panic(\"Dictionary generator has errors\")\n";
        output += std::move(errors);
    }
}
