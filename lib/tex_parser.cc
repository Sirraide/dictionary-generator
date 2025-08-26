#include <dictgen/backends.hh>
#include <base/Text.hh>

using namespace dict;

Backend::Backend(LanguageOps& ops) : ops{ops} {
    nodes.emplace_back(new EmptyNode());
}


void Renderer::render(const Node& n) {
    if (n.is<EmptyNode>()) return;
    if (auto t = n.as<TextNode>()) return render_text(t->text);
    if (auto m = n.as<MacroNode>()) return render_macro(*m);
    if (auto c = n.as<ContentNode>()) return render(c->children);
    if (auto t = n.as<ComputedTextNode>()) {
        if (t->raw) out += t->text;
        else render_text(t->text);
        return;
    }
    Unreachable("Invalid node type");
}

void Renderer::render(std::span<const Node* const> nodes) {
    for (const auto& n : nodes) render(*n);
}

auto TexParser::empty() -> Node* {
    return backend.nodes.front().get();
}

auto TexParser::parse_arg() -> Result<Node*> {
    if (not input.trim_front().starts_with('{')) return Error("Missing arg for macro");
    return ParseGroup();
}

auto TexParser::HandleUnknownMacro(std::string_view macro) -> Result<Node*> {
    return backend.ops.handle_unknown_macro(*this, macro);
}

auto TexParser::ParseContent(std::vector<Node*>& nodes, i32 braces) -> Result<> {
    while (not input.empty()) {
        nodes.push_back(make<TextNode>(input.take_until_any("\\${}")));
        switch (input.front().value_or(0)) {
            default: break;
            case '\\': nodes.push_back(Try(ParseMacro())); break;
            case '$': nodes.push_back(Try(ParseMaths())); break;

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

auto TexParser::ParseGroup() -> Result<Node*> {
    Assert(input.consume('{'), "Expected brace");
    if (input.consume('}')) return empty();
    std::vector<Node*> children;
    Try(ParseContent(children, 1));
    return make<ContentNode>(std::move(children));
}

auto TexParser::ParseMacro() -> Result<Node*> {
    Assert(input.consume('\\'), "Expected backslash");
    if (input.empty()) return Error("Invalid macro escape sequence");

    // Found a macro; first, handle single-character macros.
    if (text::IsPunct(*input.front()) or input.starts_with(' ')) {
        static constexpr std::string_view SpecialChars = "- &$%#{}";
        auto c = input.take();
        if (SpecialChars.contains(c[0])) return make<TextNode>(c);
        if (c[0] == '\\') return Error("'\\\\' is not supported in this field");
        return HandleUnknownMacro(c);
    }

    // Handle regular macros. We use custom tags for some of these to
    // separate the formatting from data.
    auto macro = input.take_while_any("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ@");
    if (macro.empty()) return Error("Invalid macro escape sequence");
    input.trim_front();

    // Builtin macros.
    if (macro == "s") return ParseSingleArgumentMacro(Macro::SmallCaps);
    if (macro == "w") return ParseSingleArgumentMacro(Macro::Lemma);
    if (macro == "textit") return ParseSingleArgumentMacro(Macro::Italic);
    if (macro == "textbf") return ParseSingleArgumentMacro(Macro::Bold);
    if (macro == "textnf") return ParseSingleArgumentMacro(Macro::Normal);
    if (macro == "senseref") return ParseSingleArgumentMacro(Macro::Sense);
    if (macro == "Sup") return ParseSingleArgumentMacro(Macro::Superscript);
    if (macro == "Sub") return ParseSingleArgumentMacro(Macro::Subscript);
    if (macro == "par") return make<MacroNode>(Macro::ParagraphBreak);
    if (macro == "ldots") return make<MacroNode>(Macro::Ellipsis);
    if (macro == "this") return make<MacroNode>(Macro::This);
    if (macro == "ref" or macro == "label" ) {
        (void) ParseGroup(); // Throw away the argument.
        return empty();
    }

    // Already handled when we split senses and examples.
    if (macro == "ex" or macro == "comment")
        return empty();

    // User-defined macro.
    return HandleUnknownMacro(macro);
}

auto TexParser::ParseMaths() -> Result<Node*> {
    Assert(input.consume('$'), "Expected '$'");
    auto node = make<ComputedTextNode>(std::format("${}$", input.take_until('$'))); // TODO: Actually support maths.
    if (not input.consume('$')) return Error("Unterminated maths");
    return node;
}

auto TexParser::Parse(Backend& backend, stream input) -> Result<Node*> {
    TexParser parser{backend, input};
    std::vector<Node*> children;
    while (not parser.input.empty()) Try(parser.ParseContent(children, 0));
    return parser.make<ContentNode>(std::move(children));
}

auto TexParser::ParseSingleArgumentMacro(Macro m) -> Result<Node*> {
    // Drop everything until the argument brace. We’re not a LaTeX tokeniser, so we don’t
    // support stuff like `\fract1 2`, as much as I like to write it.
    if (not input.trim_front().starts_with('{'))
        return Error("Sorry, macro arguments must be enclosed in braces");

    auto arg = Try(parse_arg());
    return make<MacroNode>(m, std::move(arg));
}

void TexParser::SaveNode(Node* n) {
    backend.nodes.emplace_back(n);
}
