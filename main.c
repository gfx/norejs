#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <JavaScriptCore/JavaScriptCore.h>
#define UNUSED __attribute__((unused))

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
jsGlobalPrint(
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
    JSStringRef jstrPrint  = JSStringCreateWithUTF8CString("print");
    JSObjectRef jfuncPrint = JSObjectMakeFunctionWithCallback(ctx, jstrPrint, jsGlobalPrint);
    JSObjectSetProperty(ctx, jobjGlobal, jstrPrint, jfuncPrint, kJSPropertyAttributeNone, NULL);
    JSStringRelease(jstrPrint);

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

