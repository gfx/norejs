#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <JavaScriptCore/JavaScriptCore.h>
#define UNUSED __attribute__((unused))

const char* commandTitle = "nore";

static void printJSError(JSContextRef ctx, JSValueRef exception);

static void
printJSValueRef(JSContextRef ctx, JSValueRef arg, FILE* fp, JSValueRef* exception) {
    JSStringRef jstr = JSValueToStringCopy(ctx, arg, exception);
    if (exception && *exception) {
        printJSError(ctx, *exception);
        return;
    }
    size_t const  maxSize = JSStringGetMaximumUTF8CStringSize(jstr);
    char* const   buffer  =  (char*)malloc(maxSize);
    size_t const  size    = JSStringGetUTF8CString(jstr, buffer, maxSize);
    fwrite(buffer, size, 1, fp);
    free(buffer);
    JSStringRelease(jstr);
}

static void
printJSError(JSContextRef ctx, JSValueRef exception) {
    fflush(stdout);
    printJSValueRef(ctx, exception, stderr, NULL);
    fputs("\n", stderr);
}

static JSValueRef
printToStdoutFunc(
    JSContextRef        ctx,
    JSObjectRef         jobj     UNUSED,
    JSObjectRef         jobjThis UNUSED,
    size_t              argLen,
    const JSValueRef    args[],
    JSValueRef*         exception) {

    for (size_t i = 0; i < argLen; ++i) {
        printJSValueRef(ctx, args[i], stdout, exception);
    }
    fputs("\n", stdout);

    return JSValueMakeUndefined(ctx);
}

static JSValueRef
printToStderrFunc(
    JSContextRef        ctx,
    JSObjectRef         jobj     UNUSED,
    JSObjectRef         jobjThis UNUSED,
    size_t              argLen,
    const JSValueRef    args[],
    JSValueRef*         exception) {

    for (size_t i = 0; i < argLen; ++i) {
        printJSValueRef(ctx, args[i], stderr, exception);
    }
    fputs("\n", stderr);

    return JSValueMakeUndefined(ctx);
}

static JSValueRef
makeJSValueFromCString(JSContextRef ctx, const char* cstr) {
    JSStringRef jstr = JSStringCreateWithUTF8CString(cstr);
    JSValueRef  jval = JSValueMakeString(ctx, jstr);
    JSStringRelease(jstr);
    return jval;
}

static JSObjectRef
makeJSObjectFromCString(JSContextRef ctx, const char* cstr) {
    JSValueRef exception = NULL;
    JSObjectRef jobj = JSValueToObject(ctx, makeJSValueFromCString(ctx, cstr), &exception);
    if (exception) {
        printJSError(ctx, exception);
    }
    return jobj;
}

static void
setProperty(JSContextRef ctx, JSObjectRef obj, const char* name, JSObjectRef value) {
    JSStringRef jstrName = JSStringCreateWithUTF8CString(name);
    JSObjectSetProperty(ctx, obj, jstrName, value, kJSPropertyAttributeNone, NULL);
    JSStringRelease(jstrName);
}

static void
setFunc(JSContextRef ctx, JSObjectRef obj, const char* name, JSObjectCallAsFunctionCallback fun) {
    JSStringRef jstrName = JSStringCreateWithUTF8CString(name);
    JSObjectRef jobjFunc = JSObjectMakeFunctionWithCallback(ctx, jstrName , fun);
    JSObjectSetProperty(ctx, obj, jstrName, jobjFunc, kJSPropertyAttributeNone, NULL);
    JSStringRelease(jstrName);
}

int main(int argc, char** argv) {

    if (argc == 1) {
        // TODO: interactive mode?
        exit(0);
    }
    const char* const fileName = argv[1];

    JSValueRef exception = NULL;

    // initialize globals
    JSGlobalContextRef ctx = JSGlobalContextCreate(NULL);
    JSObjectRef jobjGlobal = JSContextGetGlobalObject(ctx);

    // setup node-like environment (console and process)

    {
        JSObjectRef jsConsole = JSObjectMake(ctx, NULL, NULL);
        setProperty(ctx, jobjGlobal, "console", jsConsole);
        setFunc(ctx, jsConsole, "log",   printToStdoutFunc);
        setFunc(ctx, jsConsole, "info",  printToStdoutFunc);
        setFunc(ctx, jsConsole, "warn",  printToStderrFunc);
        setFunc(ctx, jsConsole, "error", printToStderrFunc);
    }

    {
        JSObjectRef jsProcess = JSObjectMake(ctx, NULL, NULL);
        setProperty(ctx, jobjGlobal, "process", jsProcess);

        setProperty(ctx, jsProcess, "env", JSObjectMake(ctx, NULL, NULL));
        setProperty(ctx, jsProcess, "title", makeJSObjectFromCString(ctx, commandTitle));

        // argv
        JSValueRef jvals[argc];
        for (int i = 0; i < argc; ++i) {
            jvals[i] = makeJSValueFromCString(ctx, argv[i]);
        }
        setProperty(ctx, jsProcess, "argv", JSObjectMakeArray(ctx, argc, jvals, NULL));
    }

    JSStringRef sourceContent = NULL;
    {
        struct stat st;
        if (stat(fileName, &st) != 0) {
            perror("failed to stat");
            return 1;
        }
        size_t const fileSize = (size_t)st.st_size;
        FILE* fp = fopen(fileName, "r");
        if (! fp) {
            perror("failed to open");
            return 1;
        }
        void* const buffer = calloc(fileSize + 1, sizeof(char));
        assert(buffer);
        size_t total = 0;
        size_t nread = 0;
        while ((nread = fread(buffer + total, fileSize, sizeof(char), fp)) > 0) {
            total += nread;
        }

        if (ferror(fp)) {
            perror("failed to read");
            return 1;
        }
        sourceContent = JSStringCreateWithUTF8CString((const char*)buffer);
    }
    JSStringRef sourceFile = JSStringCreateWithUTF8CString(fileName);

    // execute script
    JSEvaluateScript(ctx, sourceContent, jobjGlobal, sourceFile, 1, &exception);
    JSStringRelease(sourceContent);
    JSStringRelease(sourceFile);

    if (exception != NULL) {
        printJSError(ctx, exception);
    }

    // finalize globals
    JSGarbageCollect(ctx);
    JSGlobalContextRelease(ctx);
    return 0;
}

