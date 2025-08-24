#ifndef DICTIONARY_GENERATOR_PARSER_HH
#define DICTIONARY_GENERATOR_PARSER_HH

#include <base/FixedVector.hh>
#include <dictgen/core.hh>

namespace dict {
class ConversionBackend;


class TexParser {
    Backend& backend;

public:
    stream input;

private:
    explicit TexParser(Backend& backend, stream input)
        : backend(backend), input(input) {}

public:
    /// Run the converter.
    static auto Parse(Backend& backend, stream input) -> Result<Node::Ptr>;

    /// Make a group node.
    auto group(auto ...nodes) -> Node::Ptr {
        std::vector<Node::Ptr> children;
        (children.push_back(std::move(nodes)), ...);
        return Make<ContentNode>(std::move(children));
    }

    /// Parse a group. This can be invoked by macro handlers to parse macro arguments.
    auto parse_arg() -> Result<Node::Ptr>;

    /// Make a text node.
    ///
    /// If 'raw' is true, the text will not be escaped again.
    auto text(std::string text, bool raw = false) -> Node::Ptr {
        return Make<ComputedTextNode>(std::move(text), raw);
    }

private:
    /// Create a node.
    template <typename NodeType, typename ...Args>
    static auto Make(Args&& ...args) -> Node::Ptr {
        return Node::Ptr(std::make_unique<NodeType>(std::forward<Args>(args)...));
    }

    auto HandleUnknownMacro(std::string_view macro) -> Result<Node::Ptr>;
    auto ParseContent(std::vector<Node::Ptr>& nodes, i32 braces) -> Result<>;
    auto ParseGroup() -> Result<Node::Ptr>;
    auto ParseMacro() -> Result<Node::Ptr>;
    auto ParseMaths() -> Result<Node::Ptr>;
    auto ParseSingleArgumentMacro(Macro m) -> Result<Node::Ptr>;
};
}

#endif // DICTIONARY_GENERATOR_PARSER_HH
