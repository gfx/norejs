#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <JavaScriptCore/JavaScriptCore.h>

#include "uv.h"

#define UNUSED __attribute__((unused))

const char* const CommandTitle   = "nore";
const char* const CommandVersion = "v0.0.1";

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
    fwrite(buffer, size - 1 /* because nul included */, 1, fp);
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
        if ((i + 1) < argLen) {
            fputs(" ", stdout);
        }
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
        if ((i + 1) < argLen) {
            fputs(" ", stderr);
        }
    }
    fputs("\n", stderr);

    return JSValueMakeUndefined(ctx);
}

static JSValueRef
setTimeoutFunc( // TODO use an actual event loop like UV
    JSContextRef        ctx,
    JSObjectRef         jobj     UNUSED,
    JSObjectRef         jobjThis,
    size_t              argLen,
    const JSValueRef    args[], /* (func, duration) */
    JSValueRef*         exception) {

    if (argLen != 2) {
        return JSValueMakeUndefined(ctx);
    }

    JSObjectCallAsFunction(ctx, JSValueToObject(ctx, args[0], exception), jobjThis, 0, NULL, exception);

    return JSValueMakeUndefined(ctx);
}

// ---

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

static JSValueRef
getMemoryUsage(
    JSContextRef        ctx,
    JSObjectRef         jobj     UNUSED,
    JSObjectRef         jobjThis UNUSED,
    size_t              argLen UNUSED,
    const JSValueRef    args[] UNUSED,
    JSValueRef*         exception) {

    size_t rss = 0;
    int const err UNUSED = uv_resident_set_memory(&rss); // see node/src/node.cc

    JSObjectRef const retval = JSObjectMake(ctx, NULL, NULL);
    setProperty(ctx, retval, "rss", JSValueToObject(ctx, JSValueMakeNumber(ctx, rss), exception));

    return retval;
}


static void
skipShebang(FILE* const fp) {
    int c = fgetc(fp);
    if (c == EOF) {
        return;
    }

    if (c == '#') { // ignore shebang
        while ((c = fgetc(fp)) != EOF) {
            if (c == '\r' || c == '\n') {
                ungetc(c, fp);
                return;
            }
        }
    }
    else {
        ungetc(c, fp);
    }
}

static void
setupJSGlobals(JSContextRef ctx, JSObjectRef jobjGlobal, int argc, const char** argv) {
    // console
    JSObjectRef jsConsole = JSObjectMake(ctx, NULL, NULL);
    setProperty(ctx, jobjGlobal, "console", jsConsole);
    setFunc(ctx, jsConsole, "log",   printToStdoutFunc);
    setFunc(ctx, jsConsole, "info",  printToStdoutFunc);
    setFunc(ctx, jsConsole, "warn",  printToStderrFunc);
    setFunc(ctx, jsConsole, "error", printToStderrFunc);

    // process
    JSObjectRef jsProcess = JSObjectMake(ctx, NULL, NULL);
    setProperty(ctx, jobjGlobal, "process", jsProcess);

    setProperty(ctx, jsProcess, "env", JSObjectMake(ctx, NULL, NULL));
    setProperty(ctx, jsProcess, "title",   makeJSObjectFromCString(ctx, CommandTitle));
    setProperty(ctx, jsProcess, "version", makeJSObjectFromCString(ctx, CommandVersion));
    setFunc(ctx, jsProcess, "memoryUsage", getMemoryUsage);

    // process.argv
    JSValueRef jvals[argc];
    for (int i = 0; i < argc; ++i) {
        jvals[i] = makeJSValueFromCString(ctx, argv[i]);
    }
    setProperty(ctx, jsProcess, "argv", JSObjectMakeArray(ctx, argc, jvals, NULL));

    // setTimeout()
    setFunc(ctx, jobjGlobal, "setTimeout",   setTimeoutFunc);
}

static JSStringRef readSourceFile(JSContextRef ctx UNUSED, const char* fileName) {
    struct stat st;
    if (stat(fileName, &st) != 0) {
        perror("failed to stat");
        return NULL;
    }
    size_t const fileSize = (size_t)st.st_size;
    FILE* fp = fopen(fileName, "r");
    if (! fp) {
        perror("failed to open");
        return NULL;
    }
    void* const buffer = calloc(fileSize + 1, sizeof(char));
    assert(buffer);
    size_t total = 0;
    size_t nread = 0;

    skipShebang(fp);

    while ((nread = fread(buffer + total, fileSize, sizeof(char), fp)) > 0) {
        total += nread;
    }

    if (ferror(fp)) {
        perror("failed to read");
        return NULL;
    }
    return JSStringCreateWithUTF8CString((const char*)buffer);
}

static void usage() {
    printf("Usage:\n");
    printf("    %s file args...\n", CommandTitle);
}

int main(int argc, const char** argv) {

    if (argc == 1) {
        usage();
        return 0;
    }
    const char* const fileName = argv[1];

    int exitStatus = 0;

    // initialize globals
    JSGlobalContextRef ctx = JSGlobalContextCreate(NULL);
    JSObjectRef jobjGlobal = JSContextGetGlobalObject(ctx);

    setupJSGlobals(ctx, jobjGlobal, argc, argv);

    JSStringRef sourceContent = readSourceFile(ctx, fileName);
    if (sourceContent != NULL) {
        JSStringRef sourceFile = JSStringCreateWithUTF8CString(fileName);
        JSValueRef exception = NULL;

        // execute script
        JSEvaluateScript(ctx, sourceContent, jobjGlobal, sourceFile, 1, &exception);

        if (exception != NULL) {
            printJSError(ctx, exception);
            exitStatus = 1;
        }

        JSStringRelease(sourceFile);
        JSStringRelease(sourceContent);
    }
    else {
        exitStatus = 1;
    }

    // finalize globals
    JSGarbageCollect(ctx);
    JSGlobalContextRelease(ctx);
    return exitStatus;
}

