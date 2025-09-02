#include <base/Text.hh>
#include <dictgen/frontend.hh>
#include <print>

using namespace dict;

namespace {
constexpr str32 SenseMacroU32 = U"\\\\";
constexpr str32 Apostrophe = U"'`’\N{MODIFIER LETTER APOSTROPHE}";

auto FullStopDelimited(str32 text) -> std::string {
    text.trim();
    if (text.empty()) return "";
    auto str = text::ToUTF8(text);

    // Skip past quotes so we don’t turn e.g. ⟨...’⟩ into ⟨...’.⟩.
    while (text.ends_with_any(Apostrophe)) text.drop_back();

    // Recognise common punctuation marks.
    if (not text.ends_with_any(U"?!.") and not text.ends_with(U"\\ldots")) str += ".";
    return str;
}
}

void Entry::emit(Backend& backend) const { // clang-format off
    backend.line = line;
    auto s = text::ToUTF8(word);
    data.visit(utils::Overloaded{
        [&](const RefEntry& ref) { backend.emit(s, ref); },
        [&](const FullEntry& f)  { backend.emit(s, f); },
    });
} // clang-format on

void Generator::create_full_entry(std::u32string word, std::vector<std::u32string> parts) {
    using enum FullEntry::Part;
    FullEntry entry;

    if (not disallow_specials(word, "in the lemma"))
        return;

    // Preprocessing.
    if (auto res = ops().preprocess_full_entry(parts); not res) {
        backend.error("Preprocessing error: {}", res.error());
        return;
    }

    // Make sure we have enough parts.
    if (parts.size() < +MinParts) {
        backend.error("An entry must have at least 4 parts: word, part of speech, etymology, definition");
        return;
    }

    // Make sure we don’t have too many parts.
    if (parts.size() > +MaxParts) {
        backend.error("An entry must have at most 6 parts: word, part of speech, etymology, definition, forms, IPA");
        return;
    }

    // Process the entry. This inserts things that are difficult to do in LaTeX, such as
    // full stops between senses, only if there isn’t already a full stop there. Of course,
    // this means we need to convert that to HTML for the JSON output, but we need to do
    // that anyway since the input is already LaTeX.
    static_assert(+MaxParts == 5, "Handle all parts below");

    // Part of speech.
    entry.pos = text::ToUTF8(parts[+POSPart]);

    // Etymology.
    entry.etym = text::ToUTF8(parts[+EtymPart]);

    // Definition and senses.
    //
    // If the definition contains senses, delimit each one with a dot. We
    // do this here because there isn’t really a good way to do that
    // in LaTeX.
    //
    // A sense may contain a comment and examples; each example may also
    // contain a comment. E.g.:
    //
    // \\ sense 1
    //     \comment foo
    //     \ex example 1
    //          \comment comment for example 1
    //     \ex example 2
    //          \comment comment for example 2
    auto SplitSense = [&](str32 sense) {
        static constexpr str32 Ex = U"\\ex";
        static constexpr str32 Comment = U"\\comment";
        static auto CommentOrEx = u32regex::create(U"\\\\(?:ex|comment)").value();

        // Find the sense comment or first example, if any, and depending on which comes first.
        FullEntry::Sense s;
        auto def_text = sense.trim_front().take_until(CommentOrEx);
        bool def_is_empty = def_text.trim().empty();
        s.def = FullStopDelimited(def_text);

        // Sense has a comment.
        if (sense.trim_front().consume(Comment)) {
            if (def_is_empty) backend.error(
                "\\comment is not allowed in an empty sense or empty primary definition. Use \\textit{{...}} instead."
            );

            s.comment = FullStopDelimited(sense.trim_front().take_until(Ex));
        }

        // At this point, we should either be at the end or at an example.
        while (sense.trim_front().consume(Ex)) {
            if (def_is_empty) backend.error(
                "\\ex is not allowed in an empty sense or empty primary definition."
            );

            auto& ex = s.examples.emplace_back();
            ex.text = FullStopDelimited(sense.trim_front().take_until(CommentOrEx));
            if (sense.consume(Comment))
                ex.comment = FullStopDelimited(sense.trim_front().take_until(Ex));
        }

        // Two comments are invalid.
        if (sense.trim_front().starts_with(Comment))
            backend.error("Unexpected \\comment token");

        return s;
    };

    // Process the primary definition. This is everything before the first sense
    // and doesn’t count as a sense because it is either the only one or, if there
    // are multiple senses, it denotes a more overarching definition that applies
    // to all or most senses.
    str32 s{parts[+DefPart]};
    entry.primary_definition = SplitSense(s.take_until_and_drop(SenseMacroU32));
    for (auto sense : s.split(SenseMacroU32))
        entry.senses.push_back(SplitSense(sense));

    // Forms.
    //
    // FIXME: The dot should be added here instead of by LaTeX.
    if (parts.size() > +FormsPart) entry.forms = text::ToUTF8(parts[+FormsPart]);

    // IPA.
    if (parts.size() > +IPAPart) entry.ipa = text::ToUTF8(parts[+IPAPart]);

    // Create a canonicalised form of this entry for sorting.
    auto nfkd = transliterator(word);
    entries.emplace_back(std::move(word), backend.line, std::move(nfkd), std::move(entry));
}

bool Generator::disallow_specials(str32 text, str message) {
    auto Disallow = [&](str32 what) {
        if (text.contains(what)) {
            backend.error("'{}' cannot be used {}", what, message);
            return false;
        }

        return true;
    };

    return Disallow(U"\\ex") and Disallow(U"\\comment") and Disallow(U"\\\\");
}

auto Generator::emit_to_string() -> EmitResult {
    // Sort the entries.
    rgs::stable_sort(entries, [](const auto& a, const auto& b) {
        return a.nfkd == b.nfkd ? a.word < b.word : a.nfkd < b.nfkd;
    });

    // Emit each entry.
    for (auto& entry : entries) entry.emit(backend);
    backend.finish();
    return {backend.output, backend.has_error};
}

int Generator::emit() {
    auto [output, has_error] = emit_to_string();
    if (has_error) {
        std::println(stderr, "{}", output);
        return 1;
    }

    std::println("{}", output);
    return 0;
}

void Generator::parse(str input_text) {
    static constexpr str32 ws = U" \t\v\f\n\r";

    // Convert text to u32.
    std::u32string text = text::ToUTF32(input_text);

    // Convert a line into an entry.
    std::u32string logical_line;
    auto ShipOutLine = [&] {
        if (logical_line.empty()) return;
        defer { logical_line.clear(); };
        logical_line = str32(logical_line).fold_ws();
        str32 line{logical_line};
        line.trim();

        // If the line contains no '|' characters and a `>`,
        // it is a reference. Split by '>'. The lhs is a
        // comma-separated list of references, the rhs is the
        // actual definition.
        if (not line.contains(U'|')) {
            if (not line.contains(U'>')) {
                backend.error("An entry must contain at least one '|' or '>'");
                return;
            }

            if (not disallow_specials(line, "in a reference entry"))
                return;

            auto from = line.take_until(U'>').trim();
            auto target = line.drop().trim();
            for (auto entry : from.split(U",")) {
                auto word = entry.trim();
                entries.emplace_back(
                    std::u32string{word},
                    backend.line,
                    transliterator(word),
                    RefEntry{text::ToUTF8(target)}
                );
            }
        }

        // Otherwise, the line is an entry. Split by '|' and emit
        // a single entry for the line.
        else {
            bool first = true;
            std::u32string word;
            std::vector<std::u32string> line_parts;
            for (auto part : line.split(U"|")) {
                if (first) {
                    first = false;
                    word = std::u32string{part.trim()};
                } else {
                    line_parts.emplace_back(part.trim());
                }
            }
            create_full_entry(std::move(word), std::move(line_parts));
        }
    };

    // Process the text.
    bool skipping = false;
    for (auto [i, line] : utils::enumerate(str32(text).lines())) {
        line = line.take_until(U'#');
        backend.line = i + 1;

        // Skip empty lines.
        if (line.empty()) continue;

        // Check for directives.
        if (line.starts_with(U'$')) {
            ShipOutLine(); // Lines can’t span directives.
            if (line.consume(U"$backend")) {
                line.trim_front();
                if (line.consume(U"all")) skipping = false;
                else if (line.consume(U"json")) skipping = not dynamic_cast<JsonBackend*>(&backend);
                else if (line.consume(U"tex")) skipping = not dynamic_cast<TeXBackend*>(&backend);
                else backend.error("Unknown backend: {}", line);
                continue;
            }

            backend.error("Unknown directive: {}", line);
            continue;
        }

        // Skip lines that are not for this backend.
        if (skipping) continue;

        // Perform line continuation.
        if (line.starts_with_any(U" \t")) {
            logical_line += ' ';
            logical_line += line.trim();
            continue;
        }

        // This line starts a new entry, so ship out the last
        // one and start a new one.
        ShipOutLine();
        logical_line = line.string();
    }

    // Ship out the last line.
    ShipOutLine();
}

