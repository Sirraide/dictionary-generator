#include <dictgen/backends.hh>
#include <base/Text.hh>
#include <print>
#include <set>

using namespace dict;

namespace {
// IMPORTANT: Remember to update the function with the same name in the
// code for the ULTRAFRENCH dictionary page on nguh.org if the output of
// this function changes.
auto NormaliseForSearch(std::string_view value) -> std::string {
    // Do all filtering in UTF32 land since we need to iterate over entire characters.
    auto haystack = text::ToUTF8(
        text::Normalise(text::ToLower(text::ToUTF32(value)), text::NormalisationForm::NFKD)                                //
        | vws::filter([](char32_t c) { return U"abcdefghijklłmnopqrstuvwxyzABCDEFGHIJKLŁMNOPQRSTUVWXYZ "sv.contains(c); }) //
        | rgs::to<std::u32string>()
    );

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
}

JsonBackend::JsonBackend(LanguageOps& ops, bool minify)
    : Backend{ops}, minify{minify} {
    out = json::object();
    refs() = json::array();
    entries() = json::array();
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

void JsonBackend::emit_all() {
    if (has_error) output = std::move(errors);
    else output = minify ? out.dump() : out.dump(4);
}
