#ifndef PARSER_HH
#define PARSER_HH

#include <base/Base.hh>
#include <base/Text.hh>
#include <dictgen/backends.hh>

namespace dict {
using namespace base;
struct Entry {
    /// Headword.
    std::u32string word;

    /// Line this entry starts on.
    i64 line = 0;

    /// Headword in NFKD for sorting.
    std::u32string nfkd;

    /// Data.
    Variant<RefEntry, FullEntry> data;

    void emit(Backend& backend) const;
};

class Generator {
    /// Backend that we’re emitting code to.
    Backend& backend;

    /// Entries we have parsed.
    std::vector<Entry> entries;

    /// A transliterator used to normalise headwords for sorting.
    text::Transliterator transliterator{"NFKD; [:M:] Remove; [:Punctuation:] Remove; NFC; Lower;"};

public:
    explicit Generator(Backend& backend) : backend(backend) {}
    void emit();
    void parse(std::string_view input_text);

private:
    void create_full_entry(std::u32string word, std::vector<std::u32string> parts);
    [[nodiscard]] auto ops() -> LanguageOps& { return backend.ops; }
};
} // namespace dict

#endif // PARSER_HH
