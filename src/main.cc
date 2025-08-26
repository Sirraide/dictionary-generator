#include "dictgen-v8.hh"

#include <libplatform/libplatform.h>

using namespace dict;
using namespace dict::gen;

static constexpr std::string_view preamble = R"js(
console.log = console.error = console.debug = __print__;
globalThis.print = __print__;
)js";

static void Impl___print__(const FunctionCallbackInfo<Value>& info) {
    auto isolate = Isolate();
    for (int i = 0; i < info.Length(); i++) {
        if (i != 0) std::print(", ");
        auto a = info[i];
        String::Utf8Value v{isolate, a};
        std::print("{}", std::string_view{*v, usz(v.length())});
    }

    std::println();
    info.GetReturnValue().SetUndefined();
}

static void InitGlobal(Local<v8::ObjectTemplate> global) {
    auto i = Isolate();
    global->Set(i, "__print__", v8::FunctionTemplate::New(i, Impl___print__));
}

int main(int argc, char** argv) {
    v8::V8::InitializeICUDefaultLocation(argv[0]);
    v8::V8::InitializeExternalStartupData(argv[0]);
    std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    v8::Isolate* isolate = v8::Isolate::New(create_params);

    defer {
        isolate->Dispose();
        v8::V8::Dispose();
        v8::V8::DisposePlatform();
        delete create_params.array_buffer_allocator;
    };

    {
        v8::Isolate::Scope _{isolate};
        HandleScope _{isolate};
        auto global_template = v8::ObjectTemplate::New(isolate);
        InitGlobal(global_template);
        auto ctx = v8::Context::New(isolate, nullptr, global_template);
        v8::Context::Scope _{ctx};
        Assert(CompileAndRun(preamble, "<preamble>"));
        if (auto res = Main(argc, argv); not res.has_value()) {
            std::println(stderr, "Error: {}", res.error());
            return 1;
        }
    }
}
