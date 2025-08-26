#ifndef DICTIONARY_GENERATOR_PARSER_HH
#define DICTIONARY_GENERATOR_PARSER_HH

#include <base/FixedVector.hh>
#include <dictgen/backends.hh>
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
    static auto Parse(Backend& backend, stream input) -> Result<Node*>;

    /// Make an empty node.
    auto empty() -> Node*;

    /// Make a group node.
    auto group(auto ...nodes) -> Node* {
        std::vector<Node*> children;
        (children.push_back(std::move(nodes)), ...);
        return make<ContentNode>(std::move(children));
    }

    /// Create a node.
    template <typename NodeType, typename ...Args>
    auto make(Args&& ...args) -> Node* {
        auto n = new NodeType(std::forward<Args>(args)...);
        SaveNode(n);
        return n;
    }

    /// Parse a group. This can be invoked by macro handlers to parse macro arguments.
    auto parse_arg() -> Result<Node*>;

    /// Make a text node.
    ///
    /// If 'raw' is true, the text will not be escaped again.
    auto text(std::string text, bool raw = false) -> Node* {
        return make<ComputedTextNode>(std::move(text), raw);
    }

private:
    auto HandleUnknownMacro(std::string_view macro) -> Result<Node*>;
    auto ParseContent(std::vector<Node*>& nodes, i32 braces) -> Result<>;
    auto ParseGroup() -> Result<Node*>;
    auto ParseMacro() -> Result<Node*>;
    auto ParseMaths() -> Result<Node*>;
    auto ParseSingleArgumentMacro(Macro m) -> Result<Node*>;
    void SaveNode(Node* n);
};
}

#endif // DICTIONARY_GENERATOR_PARSER_HH
