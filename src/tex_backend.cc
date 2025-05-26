#include <dictgen/backends.hh>
#include <print>

using namespace dict;

TeXBackend::TeXBackend(LanguageOps& ops, std::string filename) : Backend{ops} {
    print("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
    print("%%            This file was generated from {}\n", filename);
    print("%%\n");
    print("%%                         DO NOT EDIT\n");
    print("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
    print("\n");
}

void TeXBackend::emit(std::string_view word, const FullEntry& data) { // clang-format off
    auto FormatSense = [](const FullEntry::Sense& s) {
        return s.def
            + (
                s.comment.empty()
                ? ""s
                : std::format(" {{\\itshape{{}}{}}}", s.comment)
            )
            + (
                s.examples.empty()
                ? ""s
                : s.examples | vws::transform([](const FullEntry::Example& ex) {
                    auto s = std::format("\\ex {}", ex.text);
                    if (not ex.comment.empty()) s += std::format(" {{\\itshape{{}}{}}}", ex.comment);
                    return s;
                }) | vws::join | rgs::to<std::string>()
            );
    };

    print(
        "\\entry{{{}}}{{{}}}{{{}}}{{{}{}}}{{{}}}\n",
        word,
        data.pos,
        data.etym,
        FormatSense(data.primary_definition),
        data.senses.empty() ? ""s : "\\\\"s + utils::join(
            data.senses,
            "\\\\",
            "{}",
            FormatSense
        ),
        data.forms
    ); // clang-format on
}

void TeXBackend::emit(std::string_view word, const RefEntry& data) {
    print("\\refentry{{{}}}{{{}}}\n", word, data);
}

void TeXBackend::emit_error(std::string error) {
    print("\\ULTRAFRENCHERERROR{{ ERROR: {} }}\n", error);
}
