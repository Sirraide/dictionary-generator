#include <dictgen/backends.hh>
#include <base/Text.hh>

using namespace dict;

void TeXToHtmlConverter::Append(std::string_view s) {
    // We need to escape certain chars for HTML. Do this first
    // since we’ll be inserting HTML tags later.
    if (not plain_text_output and not suppress_output and stream{s}.contains_any("<>§~")) {
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

auto TeXToHtmlConverter::DropArg() -> Result<> {
    tempset suppress_output = true;
    Try(ParseGroup());
    return {};
}

void TeXToHtmlConverter::DropEmptyAndAppendRaw(std::string_view s) {
    (void) input.consume("{}");
    if (not plain_text_output) AppendRaw(s);
}

auto TeXToHtmlConverter::GetArg() -> Result<std::string> {
    if (not input.trim_front().starts_with('{')) return Error("Missing arg for macro");

    // Parse the argument; note that the existing code emits to 'out', so we
    // simply let it do that and then extract the argument later.
    auto output_end = out.size();
    Try(ParseGroup());

    // Done parsing. Extract the argument.
    auto arg = out.substr(output_end);
    out.resize(output_end);
    return arg;
}

auto TeXToHtmlConverter::HandleUnknownMacro(std::string_view macro) -> Result<> {
    Try(backend.ops.handle_unknown_macro(*this, macro));
    return {};
}

auto TeXToHtmlConverter::ParseContent(i32 braces) -> Result<> {
    while (not input.empty()) {
        Append(input.take_until_any("\\${}"));
        switch (input.front().value_or(0)) {
            default: break;
            case '\\': Try(ParseMacro()); break;
            case '$': Try(ParseMaths()); break;
            case '{': braces++; break;
            case '}':
                braces--;
                if (braces == 0) return {};
                if (braces < 0) return Error("Too many '}}'s!");
                break;
        }
    }

    return Error("Unexpected end of input. Did you forget a '}}'?");
}


auto TeXToHtmlConverter::ParseGroup() -> Result<> {
    Assert(input.consume('{'), "Expected brace");
    if (input.consume('}')) return {}; // Empty group.
    return ParseContent(1);
}

auto TeXToHtmlConverter::ParseMacro() -> Result<> {
    tempset suppress_output = plain_text_output;
    Assert(input.consume('\\'), "Expected backslash");
    if (input.empty()) return Error("Invalid macro escape sequence");

    // Found a macro; first, handle single-character macros.
    if (text::IsPunct(*input.front()) or input.starts_with(' ')) {
        switch (auto c = input.take()[0]) {
            // Discretionary hyphen.
            case '-': AppendRaw("&shy;"); return {};

            // Space.
            case ' ': AppendRaw(" "); return {};

            // Escaped characters.
            case '&': AppendRaw("&amp;"); return {};
            case '$': AppendRaw("$"); return {};
            case '%': AppendRaw("%"); return {};
            case '#': AppendRaw("#"); return {};
            case '{': AppendRaw("{"); return {};
            case '}': AppendRaw("}"); return {};

            // These should no longer exist at this point.
            case '\\': return Error("'\\\\' is not supported in this field");

            // Unknown.
            default: return HandleUnknownMacro({&c, 1});
        }
    }

    // Handle regular macros. We use custom tags for some of these to
    // separate the formatting from data.
    auto macro = input.take_while_any("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ@");
    input.trim_front();
    if (macro == "pf") Try(SingleArgumentMacroToTag("uf-pf"));
    else if (macro == "s") Try(SingleArgumentMacroToTag("uf-s"));
    else if (macro == "w") Try(SingleArgumentMacroToTag("uf-w"));
    else if (macro == "textit") Try(SingleArgumentMacroToTag("em"));
    else if (macro == "textbf") Try(SingleArgumentMacroToTag("strong"));
    else if (macro == "textnf") Try(SingleArgumentMacroToTag("uf-nf"));
    else if (macro == "senseref") Try(SingleArgumentMacroToTag("uf-sense"));
    else if (macro == "Sup") Try(SingleArgumentMacroToTag("sup"));
    else if (macro == "Sub") Try(SingleArgumentMacroToTag("sub"));
    else if (macro == "par") AppendRaw("</p><p>");
    else if (macro == "L") DropEmptyAndAppendRaw("<uf-mut><sup>L</sup></uf-mut>");
    else if (macro == "N") DropEmptyAndAppendRaw("<uf-mut><sup>N</sup></uf-mut>");
    else if (macro == "ref" or macro == "label") Try(DropArg());
    else if (macro == "ldots") DropEmptyAndAppendRaw("&hellip;");
    else if (macro == "this") DropEmptyAndAppendRaw(std::format("<uf-w>{}</uf-w>", backend.current_word)); // This has already been escaped; don’t escape it again.
    else if (macro == "ex" or macro == "comment") {} // Already handled when we split senses and examples.
    else Try(HandleUnknownMacro(macro));
    return {};
}

auto TeXToHtmlConverter::ParseMaths() -> Result<> {
    Assert(input.consume('$'), "Expected '$'");
    AppendRaw("$");
    Append(input.take_until_and_drop('$'));
    AppendRaw("$");
    return {};
}


auto TeXToHtmlConverter::Run() -> Result<std::string> {
    while (not input.empty()) Try(ParseContent(0));
    return out;
}

auto TeXToHtmlConverter::SingleArgumentMacroToTag(std::string_view tag_name) -> Result<> {
    // Drop everything until the argument brace. We’re not a LaTeX tokeniser, so we don’t
    // support stuff like `\fract1 2`, as much as I like to write it.
    if (not input.trim_front().starts_with('{'))
        return Error("Sorry, macro arguments must be enclosed in braces");

    AppendRaw(std::format("<{}>", tag_name));
    Try(ParseGroup());
    AppendRaw(std::format("</{}>", tag_name));
    return {};
}

auto JsonBackend::tex_to_html(stream input, bool plain_text_output) -> std::string {
    auto res = TeXToHtmlConverter(*this, input, plain_text_output).Run();
    if (not res) {
        error("{}", res.error());
        return "<error>";
    }
    return res.value();
}
