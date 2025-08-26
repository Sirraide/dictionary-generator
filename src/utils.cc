#include "dictgen-v8.hh"

using namespace dict;
using namespace dict::gen;

auto gen::CompileAndRun(std::string_view sv, std::string_view script_name) -> Result<Local<Value>> {
    auto i = Isolate();
    auto ctx = i->GetCurrentContext();
    v8::ScriptOrigin origin{i, Str(script_name)};
    auto script = v8::Script::Compile(ctx, Str(sv), &origin);
    if (script.IsEmpty()) return Error("Script compilation failed");
    auto res = script.ToLocalChecked()->Run(ctx);
    if (res.IsEmpty()) return Error("Script evaluation failed");
    return res.ToLocalChecked();
}

auto gen::Isolate() -> v8::Isolate* {
    return v8::Isolate::GetCurrent();
}

auto gen::Str(std::string_view sv) -> Local<String> {
    auto s = String::NewFromUtf8(
        Isolate(),
        sv.data(),
        v8::NewStringType::kNormal,
        int(sv.size())
    );

    return s.ToLocalChecked();
}

auto gen::ToString(Local<Value> s) -> std::string {
    String::Utf8Value v{Isolate(), s};
    return std::string(*v, usz(v.length()));
}
