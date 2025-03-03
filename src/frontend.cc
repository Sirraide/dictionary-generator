#include <base/Text.hh>
#include <dictgen/frontend.hh>

using namespace dict;

namespace {
constexpr std::u32string_view SenseMacroU32 = U"\\\\";
constexpr std::u32string_view Apostrophe = U"'`’\N{MODIFIER LETTER APOSTROPHE}";

auto FullStopDelimited(u32stream text) -> std::string {
    text.trim();
    if (text.empty()) return "";
    auto str = text::ToUTF8(text.text());

    // Skip past quotes so we don’t turn e.g. ⟨...’⟩ into ⟨...’.⟩.
    while (text.ends_with_any(Apostrophe)) text.drop_back();

    // Recognise common punctuation marks.
    if (not text.ends_with_any(U"?!.") and not text.ends_with(U"\\ldots")) str += ".";
    return str;
}
}

Generator::Generator(Backend& backend) : backend{backend} {
    UErrorCode err{U_ZERO_ERROR};
    transliterator = icu::Transliterator::createInstance(
        "NFKD; [:M:] Remove; [:Punctuation:] Remove; NFC; Lower;",
        UTRANS_FORWARD,
        err
    );

    Assert(not U_FAILURE(err), "Failed to get NFKD normalizer: {}", u_errorName(err));
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
    auto SplitSense = [&](u32stream sense) {
        static constexpr std::u32string_view Ex = U"\\ex";
        static constexpr std::u32string_view Comment = U"\\comment";
        static auto CommentOrEx = u32regex::create(U"\\\\(?:ex|comment)").value();

        // Find the sense comment or first example, if any, and depending on which comes first.
        FullEntry::Sense s;
        s.def = FullStopDelimited(sense.trim_front().take_until(CommentOrEx));

        // Sense has a comment.
        if (sense.trim_front().consume(Comment))
            s.comment = FullStopDelimited(sense.trim_front().take_until(Ex));

        // At this point, we should either be at the end or at an example.
        while (sense.trim_front().consume(Ex)) {
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
    u32stream s{parts[+DefPart]};
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
    auto nfkd = normalise_for_sorting(word);
    entries.emplace_back(std::move(word), backend.line, std::move(nfkd), std::move(entry));
}

void Generator::emit() {
    // Sort the entries.
    rgs::stable_sort(entries, [](const auto& a, const auto& b) {
        return a.nfkd == b.nfkd ? a.word < b.word : a.nfkd < b.nfkd;
    });

    // Emit each entry.
    for (auto& entry : entries) entry.emit(backend);
    backend.print();
}

auto Generator::normalise_for_sorting(std::u32string_view word) const -> icu::UnicodeString {
    auto nfkd = icu::UnicodeString::fromUTF32(reinterpret_cast<const UChar32*>(word.data()), i32(word.size()));
    transliterator->transliterate(nfkd);
    return nfkd;
}

void Generator::parse(std::string_view input_text) {
    static constexpr std::u32string_view ws = U" \t\v\f\n\r";

    // Convert text to u32.
    std::u32string text = text::ToUTF32(input_text);

    // Convert a line into an entry.
    std::u32string logical_line;
    auto ShipOutLine = [&] {
        if (logical_line.empty()) return;
        defer { logical_line.clear(); };

        // Collapse whitespace into single spaces.
        for (usz pos = 0;;) {
            pos = logical_line.find_first_of(ws, pos);
            if (pos == std::u32string::npos) break;
            if (auto end = logical_line.find_first_not_of(ws, pos); end != std::u32string::npos) {
                logical_line.replace(pos, end - pos, U" ");
                pos = end;
            } else {
                logical_line.erase(pos);
                break;
            }
        }

        u32stream line{logical_line};
        line.trim();

        // If the line contains no '|' characters and a `>`,
        // it is a reference. Split by '>'. The lhs is a
        // comma-separated list of references, the rhs is the
        // actual definition.
        if (not line.contains(U'|')) {
            auto from = u32stream(line.take_until(U'>')).trim();
            auto target = line.drop().trim().text();
            for (auto entry : from.split(U",")) {
                auto word = entry.trim().text();
                entries.emplace_back(
                    std::u32string{word},
                    backend.line,
                    normalise_for_sorting(word),
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
                    word = std::u32string{part.trim().text()};
                } else {
                    line_parts.emplace_back(part.trim().text());
                }
            }
            create_full_entry(std::move(word), std::move(line_parts));
        }
    };

    // Process the text.
    bool skipping = false;
    for (auto [i, line] : u32stream(text).lines() | vws::enumerate) {
        line = line.take_until(U'#');
        backend.line = i;

        // Warn about non-typographic quotes, after comment deletion
        // because it’s technically fine to have them in comments.
        if (line.contains(U'\'')) backend.error(
            "Non-typographic quote! Please use ‘’ (and “” for nested quotes) instead!"
        );

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
                else backend.error("Unknown backend: {}", line.text());
                continue;
            }

            backend.error("Unknown directive: {}", line.text());
            continue;
        }

        // Skip lines that are not for this backend.
        if (skipping) continue;

        // Perform line continuation.
        if (line.starts_with_any(U" \t")) {
            logical_line += ' ';
            logical_line += line.trim().text();
            continue;
        }

        // This line starts a new entry, so ship out the last
        // one and start a new one.
        ShipOutLine();
        logical_line = line.text();
    }

    // Ship out the last line.
    ShipOutLine();
}

