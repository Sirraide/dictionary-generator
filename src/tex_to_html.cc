#include <dictgen/backends.hh>
#include <base/Text.hh>

using namespace dict;

void TeXToHtmlConverter::Append(std::string_view s) {
    // We need to escape certain chars for HTML. Do this first
    // since we’ll be inserting HTML tags later.
    if (not plain_text_output and stream{s}.contains_any("<>§~")) {
        auto copy = std::string{s};
        utils::ReplaceAll(copy, "<", "&lt;");
        utils::ReplaceAll(copy, ">", "&gt;");
        utils::ReplaceAll(copy, "§~", "grammar"); // FIXME: Make section references work somehow.
        utils::ReplaceAll(copy, "~", "&nbsp;");
        AppendRaw(copy);
    } else {
        AppendRaw(s);
    }
}

void TeXToHtmlConverter::AppendRaw(std::string_view s) {
    if (not suppress_output) out += s;
}

void TeXToHtmlConverter::DropArg() {
    (void) GetArg();
}

void TeXToHtmlConverter::DropArgAndAppendRaw(std::string_view s) {
    DropArg();
    if (not plain_text_output) AppendRaw(s);
}

auto TeXToHtmlConverter::GetArg() -> Result<std::string_view> {
    if (input.trim_front().starts_with('{')) return input.take_until_and_drop('}');
    return Error("Missing arg for macro");
}

void TeXToHtmlConverter::HandleUnknownMacro(std::string_view macro) {
    if (auto res = backend.ops.handle_unknown_macro(*this, macro); not res)
        backend.error("{}", res.error());
}

void TeXToHtmlConverter::ProcessMacro() {
    tempset suppress_output = plain_text_output;

    // Found a macro; first, handle single-character macros.
    if (text::IsPunct(*input.front()) or input.starts_with(' ')) {
        switch (auto c = input.take()[0]) {
            // Discretionary hyphen.
            case '-': AppendRaw("&shy;"); return;

            // Space.
            case ' ': AppendRaw(" "); return;

            // Escaped characters.
            case '&': AppendRaw("&amp;"); return;
            case '%': AppendRaw("%"); return;
            case '#': AppendRaw("#"); return;

            // These should no longer exist at this point.
            case '\\': backend.error("'\\\\' is not supported in this field"); return;

            // Unknown.
            default: return HandleUnknownMacro({&c, 1});
        }
    }

    // Handle regular macros. We use custom tags for some of these to
    // separate the formatting from data.
    auto macro = input.take_while_any("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ@");
    input.trim_front();
    if (macro == "pf") SingleArgumentMacroToTag("uf-pf");
    else if (macro == "s") SingleArgumentMacroToTag("uf-s");
    else if (macro == "w") SingleArgumentMacroToTag("uf-w");
    else if (macro == "textit") SingleArgumentMacroToTag("em");
    else if (macro == "textbf") SingleArgumentMacroToTag("strong");
    else if (macro == "textnf") SingleArgumentMacroToTag("uf-nf");
    else if (macro == "senseref") SingleArgumentMacroToTag("uf-sense");
    else if (macro == "Sup") SingleArgumentMacroToTag("sup");
    else if (macro == "Sub") SingleArgumentMacroToTag("sub");
    else if (macro == "par") AppendRaw("</p><p>");
    else if (macro == "L") DropArgAndAppendRaw("<uf-mut><sup>L</sup></uf-mut>");
    else if (macro == "N") DropArgAndAppendRaw("<uf-mut><sup>N</sup></uf-mut>");
    else if (macro == "ref" or macro == "label") DropArg();
    else if (macro == "ldots") DropArgAndAppendRaw("&hellip;");
    else if (macro == "this") DropArgAndAppendRaw(std::format("<uf-w>{}</uf-w>", backend.current_word)); // This has already been escaped; don’t escape it again.
    else if (macro == "ex" or macro == "comment") {} // Already handled when we split senses and examples.
    else return HandleUnknownMacro(macro);
}

auto TeXToHtmlConverter::Run() -> std::string {
    // Process macros.
    for (;;) {
        Append(input.take_until_any("\\$"));
        if (input.empty()) break;
        if (input.consume('$')) {
            tempset suppress_output = plain_text_output;
            backend.error("Rendering arbitrary maths in the dictionary is not supported");
            break;
        }

        // Yeet '\'.
        input.drop();
        if (input.empty()) {
            backend.error("Invalid macro escape sequence");
            break;
        }

        ProcessMacro();
    }

    return out;
}

void TeXToHtmlConverter::SingleArgumentMacroToTag(std::string_view tag_name) {
    // Drop everything until the argument brace. We’re not a LaTeX tokeniser, so we don’t
    // support stuff like `\fract1 2`, as much as I like to write it.
    if (not stream{input.take_until('{')}.trim().empty())
        backend.error("Sorry, macro arguments must be enclosed in braces");

    // Drop the opening brace.
    input.drop();

    // Everything until the next closing brace is our argument, but we also need to handle
    // nested macros properly.
    AppendRaw(std::format("<{}>", tag_name));
    while (not input.empty()) {
        auto arg = input.take_until_any("$\\}");
        Append(arg);

        // TODO: Render maths.
        if (input.consume('$')) {
            AppendRaw("$");
            Append(input.take_until('$'));
            AppendRaw("$");
            continue;
        }

        if (input.consume('}')) {
            AppendRaw(std::format("</{}>", tag_name));
            return;
        }

        input.drop();
        ProcessMacro();
    }
}

auto JsonBackend::tex_to_html(stream input, bool plain_text_output) -> std::string {
    return TeXToHtmlConverter(*this, input, plain_text_output).Run();
}
