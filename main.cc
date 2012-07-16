#include <JavaScriptCore/JavaScriptCore.h>

#define UNUSED __attribute__((unused))

static JSValueRef jsGlobalPrint(
    JSContextRef        ctx,
    JSObjectRef         jobj     UNUSED,
    JSObjectRef         jobjThis UNUSED,
    size_t              argLen,
    const JSObjectRef   args[],
    JSValueRef*         jobjExp) {

    for (size_t i = 0; i < argLen; ++i) {
        JSStringRef   jstrArg = JSValueToStringCopy(ctx, args[0], jobjExp);
        size_t const  maxSize = JSStringGetMaximumUTF8CStringSize(jstrArg);
        char* const   cstr    =  new char[maxSize];
        size_t const  size    = JSStringGetUTF8CString(jstrArg, cstr, maxSize);
        fwrite(cstr, size, 1, stdout);
        fwrite("\n", 1, 1, stdout);
        delete cstr;
        JSStringRelease(jstrArg);
    }

    return JSValueMakeUndefined(ctx);
}

int main(int argc, char** argv) {

    if (argc == 1) {
        // TODO: interactive mode?
        exit(0);
    }

    JSGlobalContextRef ctx = JSGlobalContextCreate(NULL);
    JSObjectRef jobjGlobal = JSContextGetGlobalObject(ctx);

    JSStringRef jstrPrint  = JSStringCreateWithUTF8CString("print");
    JSObjectRef jfuncPrint = JSObjectMakeFunctionWithCallback(ctx, jstrPrint, (JSObjectCallAsFunctionCallback)jsGlobalPrint);
    JSObjectSetProperty(ctx, jobjGlobal, jstrPrint, jfuncPrint, kJSPropertyAttributeNone, NULL);
    JSStringRelease(jstrPrint);

    JSStringRef jstrSource = JSStringCreateWithUTF8CString(argv[1]);
    JSEvaluateScript(ctx, jstrSource, NULL, NULL, 1, NULL);
    JSStringRelease(jstrSource);

    JSGlobalContextRelease(ctx);
    JSGarbageCollect(ctx);
    return 0;
}

