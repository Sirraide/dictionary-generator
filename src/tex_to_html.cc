#include <dictgen/backends.hh>
#include <base/Text.hh>

using namespace dict;

void TeXToHtmlConverter::append(std::string_view s) {
    // We need to escape certain chars for HTML. Do this first
    // since we’ll be inserting HTML tags later.
    if (not plain_text_output and not suppress_output and stream{s}.contains_any("<>§~")) {
        auto copy = std::string{s};
        utils::ReplaceAll(copy, "<", "&lt;");
        utils::ReplaceAll(copy, ">", "&gt;");
        utils::ReplaceAll(copy, "§~", "grammar"); // FIXME: Make section references work somehow.
        utils::ReplaceAll(copy, "~", "&nbsp;");
        append_raw(copy);
    } else {
        append_raw(s);
    }
}

void TeXToHtmlConverter::append_raw(std::string_view s) {
    if (not suppress_output) out += s;
}

auto TeXToHtmlConverter::drop_arg() -> Result<> {
    tempset suppress_output = true;
    Try(ParseGroup());
    return {};
}

void TeXToHtmlConverter::drop_empty_and_append_raw(std::string_view s) {
    (void) input.consume("{}");
    if (not plain_text_output) append_raw(s);
}

auto TeXToHtmlConverter::get_arg() -> Result<std::string> {
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
    Try(ops.handle_unknown_macro(*this, macro));
    return {};
}

auto TeXToHtmlConverter::ParseContent(i32 braces) -> Result<> {
    while (not input.empty()) {
        append(input.take_until_any("\\${}"));
        switch (input.front().value_or(0)) {
            default: break;
            case '\\': Try(ParseMacro()); break;
            case '$': Try(ParseMaths()); break;

            case '{':
                input.drop();
                braces++;
                break;

            case '}':
                input.drop();
                braces--;
                if (braces == 0) return {};
                if (braces < 0) return Error("Too many '}}'s!");
                break;
        }
    }

    // If 'braces' is initially 0, it’s possible for us to get here without
    // ever encountering a closing brace. This happens frequently if this
    // function is invoked at the top-level of the parser.
    if (braces) return Error("Unexpected end of input. Did you forget a '}}'?");
    return {};
}

auto TeXToHtmlConverter::ParseGroup() -> Result<> {
    Assert(input.consume('{'), "Expected brace");
    if (input.consume('}')) return {}; // Empty group.
    return ParseContent(1);
}

auto TeXToHtmlConverter::ParseMacro() -> Result<> {
    tempset suppress_output = suppress_output or plain_text_output;
    Assert(input.consume('\\'), "Expected backslash");
    if (input.empty()) return Error("Invalid macro escape sequence");

    // Found a macro; first, handle single-character macros.
    if (text::IsPunct(*input.front()) or input.starts_with(' ')) {
        switch (auto c = input.take()[0]) {
            // Discretionary hyphen.
            case '-': append_raw("&shy;"); return {};

            // Space.
            case ' ': append_raw(" "); return {};

            // Escaped characters.
            case '&': append_raw("&amp;"); return {};
            case '$': append_raw("$"); return {};
            case '%': append_raw("%"); return {};
            case '#': append_raw("#"); return {};
            case '{': append_raw("{"); return {};
            case '}': append_raw("}"); return {};

            // These should no longer exist at this point.
            case '\\': return Error("'\\\\' is not supported in this field");

            // Unknown.
            default: return HandleUnknownMacro({&c, 1});
        }
    }

    // Handle regular macros. We use custom tags for some of these to
    // separate the formatting from data.
    auto macro = input.take_while_any("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ@");
    if (macro.empty()) return Error("Invalid macro escape sequence");
    input.trim_front();
    if (macro == "pf") Try(single_argument_macro_to_tag("uf-pf"));
    else if (macro == "s") Try(single_argument_macro_to_tag("uf-s"));
    else if (macro == "w") Try(single_argument_macro_to_tag("uf-w"));
    else if (macro == "textit") Try(single_argument_macro_to_tag("em"));
    else if (macro == "textbf") Try(single_argument_macro_to_tag("strong"));
    else if (macro == "textnf") Try(single_argument_macro_to_tag("uf-nf"));
    else if (macro == "senseref") Try(single_argument_macro_to_tag("uf-sense"));
    else if (macro == "Sup") Try(single_argument_macro_to_tag("sup"));
    else if (macro == "Sub") Try(single_argument_macro_to_tag("sub"));
    else if (macro == "par") append_raw("</p><p>");
    else if (macro == "L") drop_empty_and_append_raw("<uf-mut><sup>L</sup></uf-mut>");
    else if (macro == "N") drop_empty_and_append_raw("<uf-mut><sup>N</sup></uf-mut>");
    else if (macro == "ref" or macro == "label") Try(drop_arg());
    else if (macro == "ldots") drop_empty_and_append_raw("&hellip;");
    else if (macro == "this") drop_empty_and_append_raw(std::format("<uf-w>{}</uf-w>", current_word)); // This has already been escaped; don’t escape it again.
    else if (macro == "ex" or macro == "comment") {} // Already handled when we split senses and examples.
    else Try(HandleUnknownMacro(macro));
    return {};
}

auto TeXToHtmlConverter::ParseMaths() -> Result<> {
    Assert(input.consume('$'), "Expected '$'");
    append_raw("$");
    append(input.take_until_and_drop('$'));
    append_raw("$");
    return {};
}

auto TeXToHtmlConverter::run() -> Result<std::string> {
    while (not input.empty()) Try(ParseContent(0));
    return out;
}

auto TeXToHtmlConverter::single_argument_macro_to_tag(std::string_view tag_name) -> Result<> {
    // Drop everything until the argument brace. We’re not a LaTeX tokeniser, so we don’t
    // support stuff like `\fract1 2`, as much as I like to write it.
    if (not input.trim_front().starts_with('{'))
        return Error("Sorry, macro arguments must be enclosed in braces");

    append_raw(std::format("<{}>", tag_name));
    Try(ParseGroup());
    append_raw(std::format("</{}>", tag_name));
    return {};
}

auto JsonBackend::tex_to_html(stream input, bool plain_text_output) -> std::string {
    auto res = TeXToHtmlConverter(ops, current_word, input, plain_text_output).run();
    if (not res) {
        error("{}", res.error());
        return "<error>";
    }
    return res.value();
}
