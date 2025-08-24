#include <dictgen/backends.hh>

using namespace dict;

struct TypstBackend::Renderer : dict::Renderer {
    TypstBackend& backend;
    Renderer(TypstBackend& backend) : backend(backend) {}
    void render_macro(const MacroNode& n) override;
    void render_text(std::string_view text) override;
};

void TypstBackend::Renderer::render_macro(const MacroNode& n) {
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
        case Macro::This:
            if (backend.current_word.empty()) backend.error("'\\this' is not allowed here");
            out += backend.current_word;
            return;
    }

    render(n.args);
    out += "]";
}

void TypstBackend::Renderer::render_text(std::string_view text) {
    out += stream(text).escape("*_`<@=-+/\\~#$");
}

auto TypstBackend::convert(stream input) -> std::string {
    auto res = TexParser::Parse(*this, input);
    if (not res.has_value()) {
        error("{}", res.error());
        return "";
    }

    Renderer r{*this};
    r.render(*res.value());
    return std::move(r.out);
}

void TypstBackend::emit(std::string_view word, const RefEntry& data) {
    output += std::format("#dictionary-reference[{}][{}]\n", word, data);
}

void TypstBackend::emit(std::string_view word, const FullEntry& data) {
    auto FormatSense = [&](const FullEntry::Sense& s) -> std::string {
        // Omit the sense if it is entirely empty.
        if (s.comment.empty() and s.examples.empty() and s.def.empty()) return "";

        // Add the definition.
        auto sense = std::format("#dictionary-sense([{}]", convert(s.def));

        // Add the comment.
        if (s.comment.empty()) sense += ",[]";
        else sense += std::format(",#dictionary-comment[{}]", convert(s.comment));

        // Add the examples.
        for (const auto& e : s.examples) {
            sense += ",";
            sense += std::format("#dictionary-example([{}]", convert(e.text));
            if (e.comment.empty()) sense += ",[]";
            else sense += std::format(",#dictionary-comment[{}]", convert(e.comment));
            sense += ")";
        }

        sense += ")";
        return sense;
    };

    current_word = word;
    output += std::format(
        "#dictionary-entry([{}],[{}],[{}],[{}],{}{})\n",
        word,
        convert(data.pos),
        convert(data.etym),
        convert(data.forms),
        FormatSense(data.primary_definition),
        utils::join(data.senses, "", ",{}", FormatSense)
    );
}

void TypstBackend::emit_error(std::string error) {
    output += std::format("#panic(\"{}\")", utils::Escape(error));
}
