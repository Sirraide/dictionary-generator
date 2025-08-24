#include <dictgen/backends.hh>
#include <base/Text.hh>
#include <print>
#include <set>

using namespace dict;

JsonBackend::JsonBackend(LanguageOps& ops, bool minify)
    : Backend{ops}, minify{minify} {
    out = json::object();
    refs() = json::array();
    entries() = json::array();
    html_escaper.add("<", "&lt;");
    html_escaper.add(">", "&gt;");
    html_escaper.add("§~", "grammar"); // FIXME: Make section references work somehow.
    html_escaper.add("~", "&nbsp;");
    html_escaper.add("-", "&shy;");
    html_escaper.add("&", "&amp;");
}

struct JsonBackend::Renderer : dict::Renderer {
    JsonBackend& backend;
    const bool strip_formatting;
    Renderer(JsonBackend& backend, bool strip_formatting)
        : backend{backend}, strip_formatting{strip_formatting} {}

    void render_macro(const MacroNode& n) override;
    void render_text(std::string_view text) override;
    auto tag_name(Macro m) -> std::string_view;
};

void JsonBackend::Renderer::render_macro(const MacroNode& n) {
    if (strip_formatting) return;
    if (auto s = tag_name(n.macro); not s.empty()) {
        out += std::format("<{}>", s);
        render(n.args);
        out += std::format("</{}>", s);
        return;
    }

    switch (n.macro) {
        default: Unreachable("Unsupported macro '{}'", enchantum::to_string(n.macro));
        case Macro::Ellipsis: out += "&hellip;"; break;
        case Macro::ParagraphBreak: out += "</p><p>"; break;
        case Macro::This:
            if (backend.current_word.empty()) backend.error("'\\this' is not allowed here");
            out += backend.current_word;
            break;
    }
}

void JsonBackend::Renderer::render_text(std::string_view text) {
    if (stream{text}.contains_any("<>§~-&")) out += backend.html_escaper.replace(text);
    else out += text;
}

auto JsonBackend::Renderer::tag_name(Macro m) -> std::string_view {
    switch (m) {
        case Macro::Bold: return "strong";
        case Macro::Italic: return "em";
        case Macro::Lemma: return "f-w";
        case Macro::Normal: return "f-nf";
        case Macro::Sense: return "f-sense";
        case Macro::SmallCaps: return "f-s";
        case Macro::Subscript: return "sub";
        case Macro::Superscript: return "sup";

        // Not a tag.
        case Macro::Ellipsis:
        case Macro::ParagraphBreak:
        case Macro::This:
            return "";
    }

    Unreachable("Invalid macro");
}

// IMPORTANT: Remember to update the function with the same name in the
// code for the ULTRAFRENCH dictionary page on nguh.org if the output of
// this function changes.
auto JsonBackend::NormaliseForSearch(std::string_view value) -> std::string {
    auto haystack = search_transliterator(value);

    // The steps below only apply to the haystack, not the needle, and should
    // NOT be applied on the frontend:
    //
    // Yeet all instances of 'sbdsth', which is what 'sbd./sth.' degenerates to.
    auto remove_weird = stream(haystack).trim().replace("sbdsth", "");

    // Trim and fold whitespace.
    auto fold_ws = stream(remove_weird).fold_ws();

    // Unique all words.
    return utils::join(
        stream(fold_ws).split(" ")                            //
            | vws::transform([](auto s) { return s.text(); }) //
            | rgs::to<std::set>(),
        " "
    );
}

void JsonBackend::emit(std::string_view word, const FullEntry& data) {
    json& e = entries().emplace_back();
    e["word"] = current_word = tex_to_html(word);
    e["pos"] = tex_to_html(data.pos);
    e["ipa"] = Normalise([&] -> std::string {
        // If the user provided IPA, use it.
        if (not data.ipa.empty()) return data.ipa;

        // Otherwise, call the conversion function.
        auto ipa = ops.to_ipa(current_word);
        if (ipa.has_value()) return std::move(ipa.value());
        error("Could not convert '{}' to IPA: {}", word, ipa.error());
        return "";
    }(), text::NormalisationForm::NFC);

    auto EmitSense = [&](const FullEntry::Sense& sense) {
        json s;
        s["def"] = tex_to_html(sense.def);
        if (not sense.comment.empty()) s["comment"] = std::format("<p>{}</p>", tex_to_html(sense.comment));
        if (not sense.examples.empty()) {
            auto& ex = s["examples"] = json::array();
            for (auto& example : sense.examples) {
                json& j = ex.emplace_back();
                j["text"] = tex_to_html(example.text);
                if (not example.comment.empty()) j["comment"] = tex_to_html(example.comment);
            }
        }
        return s;
    };

    if (not data.etym.empty()) e["etym"] = tex_to_html(data.etym);
    if (not data.primary_definition.def.empty()) e["def"] = EmitSense(data.primary_definition);
    if (not data.forms.empty()) e["forms"] = tex_to_html(data.forms);
    if (not data.senses.empty()) {
        json& senses = e["senses"] = json::array();
        for (auto& sense : data.senses) senses.push_back(EmitSense(sense));
    }

    // Precomputed normalised strings for searching.
    auto all_senses = utils::join(data.senses | vws::transform(&FullEntry::Sense::def), "");
    e["hw-search"] = NormaliseForSearch(tex_to_html(word, true));
    e["def-search"] = NormaliseForSearch(tex_to_html(data.primary_definition.def, true) + tex_to_html(all_senses, true));
}

void JsonBackend::emit(std::string_view word, const RefEntry& data) {
    json& e = refs().emplace_back();
    e["from"] = current_word = tex_to_html(word);
    e["from-search"] = NormaliseForSearch(tex_to_html(current_word, true));
    e["to"] = tex_to_html(data);
}

void JsonBackend::emit_error(std::string error) {
    errors += error;
    if (not errors.ends_with('\n')) errors += "\n";
}

void JsonBackend::finish() {
    if (has_error) output = std::move(errors);
    else output = minify ? out.dump() : out.dump(4);
}

auto JsonBackend::tex_to_html(stream input, bool strip_macros) -> std::string {
    auto res = TexParser::Parse(*this, input);
    if (not res.has_value()) {
        error("{}", res.error());
        return "";
    }

    Renderer r{*this, strip_macros};
    r.render(*res.value());
    return std::move(r.out);
}
