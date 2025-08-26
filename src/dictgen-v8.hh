#ifndef DICTIONARY_GENERATOR_DICTGEN_V8_HH
#define DICTIONARY_GENERATOR_DICTGEN_V8_HH

#include <base/Assert.hh>
#include <base/Macros.hh>
#include <base/Result.hh>
#include <print>
#include <v8.h>

namespace dict::gen {
using namespace base;

using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Local;
using v8::Persistent;
using v8::String;
using v8::Value;

namespace detail {
void ThrowImpl(std::string s);
}

auto CompileAndRun(std::string_view code, std::string_view script_name) -> Result<Local<Value>>;
auto Isolate() -> v8::Isolate*;
auto Main(int argc, char **argv) -> Result<int>;
auto Str(std::string_view sv) -> Local<String>;
auto ToString(Local<Value> s) -> std::string;

template <typename ...Args>
void Throw(std::format_string<Args...> fmt, Args&&... args) {
    detail::ThrowImpl(std::format(fmt, std::forward<Args>(args)...));
}
}

template <std::derived_from<v8::Value> T>
struct std::formatter<v8::Local<T>> : std::formatter<std::string_view> {
    auto format(v8::Local<T> l, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}", dict::gen::ToString(l));
    }
};

#endif // DICTIONARY_GENERATOR_DICTGEN_V8_HH
