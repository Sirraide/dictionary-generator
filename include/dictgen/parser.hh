#ifndef DICTIONARY_GENERATOR_PARSER_HH
#define DICTIONARY_GENERATOR_PARSER_HH

#include <base/FixedVector.hh>
#include <dictgen/core.hh>

namespace dict {
class ConversionBackend;


class TexParser {
    Backend& backend;

public:
    str input;

private:
    explicit TexParser(Backend& backend, str input)
        : backend(backend), input(input) {}

public:
    /// Run the converter.
    static auto Parse(Backend& backend, str input) -> Result<Node::Ptr>;

    /// Check what target we’re compiling for.
    template <std::derived_from<Backend> T>
    bool backend_is() {
        return dynamic_cast<T*>(&backend) != nullptr;
    }

    /// Make a group node.
    auto group(auto ...nodes) -> Node::Ptr {
        std::vector<Node::Ptr> children;
        (children.push_back(std::move(nodes)), ...);
        return Make<ContentNode>(std::move(children));
    }

    /// Parse a group. This can be invoked by macro handlers to parse macro arguments.
    auto parse_arg() -> Result<Node::Ptr>;

    /// Make a formatting node; text passed to this will be inserted literally and
    /// stripped out entirely in context were we don’t care about formatting.
    auto formatting(std::string text) -> Node::Ptr {
        return Make<FormattingNode>(std::move(text));
    }

    /// Make a text node; text passed to this will be escaped.
    auto text(std::string text) -> Node::Ptr {
        return Make<ComputedTextNode>(std::move(text));
    }

private:
    /// Create a node.
    template <typename NodeType, typename ...Args>
    static auto Make(Args&& ...args) -> Node::Ptr {
        return Node::Ptr(std::make_unique<NodeType>(std::forward<Args>(args)...));
    }

    auto HandleUnknownMacro(str macro) -> Result<Node::Ptr>;
    auto ParseContent(std::vector<Node::Ptr>& nodes, i32 braces) -> Result<>;
    auto ParseGroup() -> Result<Node::Ptr>;
    auto ParseMacro() -> Result<Node::Ptr>;
    auto ParseMaths() -> Result<Node::Ptr>;
    auto ParseSingleArgumentMacro(Macro m) -> Result<Node::Ptr>;
};
}

#endif // DICTIONARY_GENERATOR_PARSER_HH
