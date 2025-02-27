#ifndef PARSER_HH
#define PARSER_HH

#include <base/Base.hh>
#include <dictgen/backends.hh>
#include <unicode/translit.h>

namespace dict {
using namespace base;
struct Entry {
    /// Headword.
    std::u32string word;

    /// Line this entry starts on.
    i64 line = 0;

    /// Headword in NFKD for sorting.
    icu::UnicodeString nfkd;

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
    icu::Transliterator* transliterator;

public:
    Generator(Backend& backend);
    void emit();
    void parse(std::string_view input_text);

private:
    void create_full_entry(std::u32string word, std::vector<std::u32string> parts);
    auto normalise_for_sorting(std::u32string_view word) const -> icu::UnicodeString;
};
} // namespace dict

#endif // PARSER_HH
