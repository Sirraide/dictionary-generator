#include <dictgen/backends.hh>
#include <print>

using namespace dict;

TeXBackend::TeXBackend(LanguageOps& ops, std::string filename) : Backend{ops} {
    std::println("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%");
    std::println("%%            This file was generated from {}", filename);
    std::println("%%");
    std::println("%%                         DO NOT EDIT");
    std::println("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%");
    std::println();
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

    std::println(
        "\\entry{{{}}}{{{}}}{{{}}}{{{}{}}}{{{}}}",
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
    std::println("\\refentry{{{}}}{{{}}}", word, data);
}

void TeXBackend::emit_error(std::string error) {
    std::print("\\ULTRAFRENCHERERROR{{ ERROR: {} }}", error);
}
