#ifndef BACKENDS_HH
#define BACKENDS_HH

#include <base/Base.hh>
#include <base/Text.hh>
#include <nlohmann/json.hpp>

namespace dict {
using namespace base;
using nlohmann::json;

class Backend;
class JsonBackend;
class TeXToHtmlConverter;

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

/// Language-specific operations.
struct LanguageOps {
    virtual ~LanguageOps() = default;

    /// Handle an unknown macro.
    ///
    /// \param macro The macro name, *without* the leading backslash.
    virtual auto handle_unknown_macro(TeXToHtmlConverter&, std::string_view macro) -> Result<> {
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

class TeXToHtmlConverter {
    friend JsonBackend;

public:
    LanguageOps& ops;
    std::string_view current_word;
    stream input;
    bool plain_text_output;
    bool suppress_output = false;
    std::string out = "";

    /// Append HTML-escaped text.
    void append(std::string_view s);

    /// Append text without HTML-escaping.
    void append_raw(std::string_view s);

    /// Drop the next argument if present. Nested macros are not expanded.
    auto drop_arg() -> Result<>;

    /// Drop an empty argument if present and append a raw string.
    void drop_empty_and_append_raw(std::string_view s);

    /// Get the next argument, if there is one. This expands nested macros and drops braces.
    auto get_arg() -> Result<std::string>;

    /// Parse a macro and expand its contents and insert the resulting string
    /// into the output, wrapped in a tag with the given name.
    auto single_argument_macro_to_tag(std::string_view tag_name) -> Result<>;

    /// Run the converter.
    auto run() -> Result<std::string>;

private:
    auto HandleUnknownMacro(std::string_view macro) -> Result<>;
    auto ParseContent(i32 braces) -> Result<>;
    auto ParseGroup() -> Result<>;
    auto ParseMacro() -> Result<>;
    auto ParseMaths() -> Result<>;
};

class Backend {
protected:
    Backend(LanguageOps& ops) : ops{ops} {}

public:
    LanguageOps& ops;
    std::string output;

    i64 line = 0;

    bool has_error = false;

    template <std::derived_from<Backend> BackendType, typename... Args>
    static auto New(Args&&... args) -> std::unique_ptr<Backend> {
        return std::unique_ptr<Backend>{new BackendType(LIBBASE_FWD(args)...)};
    }

    // Backend-specific error processing.
    template <typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args) {
        has_error = true;
        emit_error(std::format("In Line {}: {}", line, std::format(fmt, LIBBASE_FWD(args)...)));
    }

    // Print to the output.
    template <typename... Args>
    void print(std::format_string<Args...> fmt, Args&&... args) {
        output += std::format(fmt, LIBBASE_FWD(args)...);
    }

    virtual ~Backend() = default;
    virtual void emit(std::string_view word, const RefEntry& data) = 0;
    virtual void emit(std::string_view word, const FullEntry& data) = 0;
    virtual void emit_error(std::string error) = 0;
    virtual void emit_all() = 0;
};

class JsonBackend final : public Backend {
    friend TeXToHtmlConverter;

    json out;
    std::string errors;
    std::string current_word = "<error: \\this undefined>";
    bool minify;

    /// A transliterator used to normalise headwords for searching.
    text::Transliterator search_transliterator{"NFKD; Latin-ASCII; [:M:] Remove; [:Punctuation:] Remove; NFC; Lower"};

public:
    explicit JsonBackend(LanguageOps& ops, bool minify);

    void emit(std::string_view word, const FullEntry& data) override;
    void emit(std::string_view word, const RefEntry& data) override;
    void emit_error(std::string error) override;
    void emit_all() override;

private:
    auto NormaliseForSearch(std::string_view value) -> std::string;
    auto entries() -> json& { return out["entries"]; }
    auto refs() -> json& { return out["refs"]; }
    auto tex_to_html(stream input, bool plain_text_output = false) -> std::string;
};

class TeXBackend final : public Backend {
public:
    explicit TeXBackend(LanguageOps& ops, std::string fname);

    void emit(std::string_view word, const FullEntry& data) override;
    void emit(std::string_view word, const RefEntry& data) override;

    // Emit errors as LaTeX macros.
    //
    //  This is so the error gets printed at the end of LaTeX compilation;
    //  if we print it when the program runs, it’s likely to get missed,
    //  so we do this instead.
    void emit_error(std::string error) override;

    // This backend prints immediately during generation.
    void emit_all() override {} // No-op.
};
} // namespace dict

#endif // BACKENDS_HH
