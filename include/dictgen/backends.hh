#ifndef BACKENDS_HH
#define BACKENDS_HH

#include <base/Trie.hh>
#include <dictgen/parser.hh>
#include <nlohmann/json.hpp>

namespace dict {
using nlohmann::json;

class Backend;
class JsonBackend;

class Backend {
protected:
    Backend(LanguageOps& ops) : ops{ops} {}

public:
    /// Language-specific operations.
    LanguageOps& ops;

    /// Output buffer.
    std::string output;

    /// Current line.
    i64 line = 1;

    /// Whether we’ve encountered an error,
    bool has_error = false;

    /// Temporarily suppresses any output.
    bool suppress_output = false;

    /// Create a new backend.
    template <std::derived_from<Backend> BackendType, typename... Args>
    static auto New(Args&&... args) -> std::unique_ptr<Backend> {
        return std::unique_ptr<Backend>{new BackendType(LIBBASE_FWD(args)...)};
    }

    /// Backend-specific error processing.
    template <typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args) {
        has_error = true;
        emit_error(std::format("In Line {}: {}", line, std::format(fmt, LIBBASE_FWD(args)...)));
    }

    /// Print to the output.
    template <typename... Args>
    void print(std::format_string<Args...> fmt, Args&&... args) {
        output += std::format(fmt, LIBBASE_FWD(args)...);
    }

    /// Destructor.
    virtual ~Backend() = default;

    /// Emit a reference.
    virtual void emit(str word, const RefEntry& data) = 0;

    /// Emit a full entry.
    virtual void emit(str word, const FullEntry& data) = 0;
    virtual void emit_error(std::string error) = 0;
    virtual void finish() {}
};

class JsonBackend final : public Backend {
    struct Renderer;
    trie html_escaper;
    json out;
    std::string errors;
    std::string current_word;
    bool minify;

    /// A transliterator used to normalise headwords for searching.
    text::Transliterator search_transliterator{"NFKD; Latin-ASCII; [^a-z A-Z\\ ] Remove; Lower"};

public:
    explicit JsonBackend(LanguageOps& ops, bool minify);

    void emit(str word, const FullEntry& data) override;
    void emit(str word, const RefEntry& data) override;
    void emit_error(std::string error) override;
    void finish() override;

private:
    auto NormaliseForSearch(str value) -> std::string;
    auto tex_to_html(str input, bool strip_macros = false) -> std::string;
    auto entries() -> json& { return out["entries"]; }
    auto refs() -> json& { return out["refs"]; }
    auto tag_name(Macro m) -> str;
};

class TypstBackend final : public Backend {
    friend TexParser;
    struct Renderer;
    std::string current_word;

public:
    explicit TypstBackend(LanguageOps& ops) : Backend(ops) {}

    void emit(str word, const FullEntry& data) override;
    void emit(str word, const RefEntry& data) override;
    void emit_error(std::string error) override;

private:
    auto convert(str input) -> std::string;
};

class TeXBackend final : public Backend {
public:
    explicit TeXBackend(LanguageOps& ops, std::string fname);

    void emit(str word, const FullEntry& data) override;
    void emit(str word, const RefEntry& data) override;

    // Emit errors as LaTeX macros.
    //
    //  This is so the error gets printed at the end of LaTeX compilation;
    //  if we print it when the program runs, it’s likely to get missed,
    //  so we do this instead.
    void emit_error(std::string error) override;
};
} // namespace dict

#endif // BACKENDS_HH
