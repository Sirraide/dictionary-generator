#ifndef DICTIONARY_GENERATOR_CORE_HH
#define DICTIONARY_GENERATOR_CORE_HH

#include <base/Base.hh>
#include <base/Text.hh>

namespace dict {
using namespace base;
class TexParser;
class Backend;
class Renderer;

using RefEntry = std::string;
struct FullEntry {
    struct Example {
        std::string text;
        std::string comment;
    };

    struct Sense {
        std::string def;
        std::string comment;
        std::vector<Example> examples;
    };

    // Entry parts.
    //
    // Note that the headword has already been removed from this, so the ‘first
    // part’ here is the part of speech (which is the second field in the raw
    // file) etc.
    enum struct Part {
        POSPart,
        EtymPart,
        DefPart,
        FormsPart,
        IPAPart,

        MaxParts,
        MinParts = DefPart + 1,
    };

    /// Part of speech.
    std::string pos;

    /// Etymology; may be empty.
    std::string etym;

    /// Pronunciation; may be empty.
    std::string ipa;

    /// Primary definition, before any sense actual sense. This is also used
    /// if there is only one sense.
    Sense primary_definition;

    /// Senses after the primary definition. If there are multiple
    /// senses, the primary definition is everything before the
    /// first slash and thus often empty.
    std::vector<Sense> senses;

    /// Forms. Mainly used for verbs.
    std::string forms;
};

struct [[nodiscard]] Node {
    LIBBASE_IMMOVABLE(Node);
    struct Ptr : std::unique_ptr<Node> {
        Ptr() = delete("Should never construct a 'null' node");
        explicit Ptr(std::unique_ptr<Node> base) : unique_ptr(std::move(base)) {}
    };

    virtual ~Node() = default;

    template <std::derived_from<Node> NodeTy>
    [[nodiscard]] auto as() const -> const NodeTy* {
        return dynamic_cast<const NodeTy*>(this);
    }

    template <std::derived_from<Node> NodeTy>
    [[nodiscard]] bool is() const { return as<NodeTy>() != nullptr; }

protected:
    Node() = default;
};

struct TextNode final : Node {
    std::string_view text;
    explicit TextNode(std::string_view text) : text(text) {}
};

struct ComputedTextNode final : Node {
    std::string text;
    bool raw;
    explicit ComputedTextNode(std::string text, bool raw = false)
        : text(std::move(text)), raw{raw} {}
};

/// Builtin macros.
enum class Macro {
    Bold,
    Ellipsis,
    Italic,
    Lemma, ///< Formatting used for the headword
    Normal, ///< Remove all formatting.
    ParagraphBreak,
    Sense,
    SmallCaps,
    Subscript,
    Superscript,
    This, ///< The current word.
};

struct MacroNode final : Node {
    const Macro macro;
    FixedVector<Ptr, 1> args;

    explicit MacroNode(Macro macro, FixedVector<Ptr, 1> args = {})
        : macro(macro),
          args(std::move(args)) {}

    explicit MacroNode(Macro macro, Ptr arg) : macro(macro), args{std::move(arg)} {}
};

struct ContentNode final : Node {
    std::vector<Ptr> children;
    explicit ContentNode(std::vector<Ptr> children) : children(std::move(children)) {}
};

struct EmptyNode final : Node {};

class Renderer {
public:
    std::string out;

    virtual ~Renderer() = default;

    void render(const Node& n);
    void render(std::span<const Node::Ptr> nodes);
    virtual void render_macro(const MacroNode& n) = 0;
    virtual void render_text(std::string_view text) = 0;
};

/// Language-specific operations.
struct LanguageOps {
    virtual ~LanguageOps() = default;

    /// Handle an unknown macro.
    ///
    /// \param macro The macro name, *without* the leading backslash.
    virtual auto handle_unknown_macro(TexParser&, std::string_view macro) -> Result<Node::Ptr> {
        return Error("Unsupported macro '{}'. Please add support for it to the dictionary generator.", macro);
    }

    /// Preprocess the fields before conversion is attempted.
    virtual auto preprocess_full_entry(std::vector<std::u32string>&) -> Result<> { return {}; }

    /// Convert the language’s text to IPA.
    ///
    /// This can return an empty string if we don’t care about including
    /// a phonetic representation of the word.
    [[nodiscard]] virtual auto to_ipa(std::string_view) -> Result<std::string> = 0;
};
}

#endif // DICTIONARY_GENERATOR_CORE_HH
