#include "common/printing.h"
#include "common/jsonconfig.h"
#include "detection/opengl/opengl.h"
#include "modules/opengl/opengl.h"
#include "util/stringUtils.h"

void ffPrintOpenGL(FFOpenGLOptions* options)
{
    FFOpenGLResult result;
    ffStrbufInit(&result.version);
    ffStrbufInit(&result.renderer);
    ffStrbufInit(&result.vendor);
    ffStrbufInit(&result.slv);
    ffStrbufInit(&result.library);

    const char* error = ffDetectOpenGL(options, &result);
    if(error)
    {
        ffPrintError(FF_OPENGL_MODULE_NAME, 0, &options->moduleArgs, FF_PRINT_TYPE_DEFAULT, "%s", error);
        return;
    }

    if(options->moduleArgs.outputFormat.length == 0)
    {
        ffPrintLogoAndKey(FF_OPENGL_MODULE_NAME, 0, &options->moduleArgs, FF_PRINT_TYPE_DEFAULT);
        puts(result.version.chars);
    }
    else
    {
        FF_PRINT_FORMAT_CHECKED(FF_OPENGL_MODULE_NAME, 0, &options->moduleArgs, FF_PRINT_TYPE_DEFAULT, ((FFformatarg[]) {
            FF_FORMAT_ARG(result.version, "version"),
            FF_FORMAT_ARG(result.renderer, "renderer"),
            FF_FORMAT_ARG(result.vendor, "vendor"),
            FF_FORMAT_ARG(result.slv, "slv"),
            FF_FORMAT_ARG(result.library, "library"),
        }));
    }

    ffStrbufDestroy(&result.version);
    ffStrbufDestroy(&result.renderer);
    ffStrbufDestroy(&result.vendor);
    ffStrbufDestroy(&result.slv);
    ffStrbufDestroy(&result.library);
}

bool ffParseOpenGLCommandOptions(FFOpenGLOptions* options, const char* key, const char* value)
{
    const char* subKey = ffOptionTestPrefix(key, FF_OPENGL_MODULE_NAME);
    if (!subKey) return false;
    if (ffOptionParseModuleArgs(key, subKey, value, &options->moduleArgs))
        return true;

    if (ffStrEqualsIgnCase(subKey, "library"))
    {
        options->library = (FFOpenGLLibrary) ffOptionParseEnum(key, value, (FFKeyValuePair[]) {
            { "auto", FF_OPENGL_LIBRARY_AUTO },
            { "egl", FF_OPENGL_LIBRARY_EGL },
            { "glx", FF_OPENGL_LIBRARY_GLX },
            {}
        });
        return true;
    }

    return false;
}

void ffParseOpenGLJsonObject(FFOpenGLOptions* options, yyjson_val* module)
{
    yyjson_val *key_, *val;
    size_t idx, max;
    yyjson_obj_foreach(module, idx, max, key_, val)
    {
        const char* key = yyjson_get_str(key_);
        if(ffStrEqualsIgnCase(key, "type") || ffStrEqualsIgnCase(key, "condition"))
            continue;

        if (ffJsonConfigParseModuleArgs(key, val, &options->moduleArgs))
            continue;

        if (ffStrEqualsIgnCase(key, "library"))
        {
            int value;
            const char* error = ffJsonConfigParseEnum(val, &value, (FFKeyValuePair[]) {
                { "auto", FF_OPENGL_LIBRARY_AUTO },
                { "egl", FF_OPENGL_LIBRARY_EGL },
                { "glx", FF_OPENGL_LIBRARY_GLX },
                {},
            });
            if (error)
                ffPrintError(FF_OPENGL_MODULE_NAME, 0, &options->moduleArgs, FF_PRINT_TYPE_DEFAULT, "Invalid %s value: %s", key, error);
            else
                options->library = (FFOpenGLLibrary) value;
            continue;
        }

        ffPrintError(FF_OPENGL_MODULE_NAME, 0, &options->moduleArgs, FF_PRINT_TYPE_DEFAULT, "Unknown JSON key %s", key);
    }
}

void ffGenerateOpenGLJsonConfig(FFOpenGLOptions* options, yyjson_mut_doc* doc, yyjson_mut_val* module)
{
    __attribute__((__cleanup__(ffDestroyOpenGLOptions))) FFOpenGLOptions defaultOptions;
    ffInitOpenGLOptions(&defaultOptions);

    ffJsonConfigGenerateModuleArgsConfig(doc, module, &defaultOptions.moduleArgs, &options->moduleArgs);

    if (options->library != defaultOptions.library)
    {
        switch (options->library)
        {
        case FF_OPENGL_LIBRARY_AUTO:
            yyjson_mut_obj_add_str(doc, module, "library", "auto");
            break;
        case FF_OPENGL_LIBRARY_EGL:
            yyjson_mut_obj_add_str(doc, module, "library", "egl");
            break;
        case FF_OPENGL_LIBRARY_GLX:
            yyjson_mut_obj_add_str(doc, module, "library", "glx");
            break;
        }
    }
}

void ffGenerateOpenGLJsonResult(FF_MAYBE_UNUSED FFOpenGLOptions* options, yyjson_mut_doc* doc, yyjson_mut_val* module)
{
    FFOpenGLResult result;
    ffStrbufInit(&result.version);
    ffStrbufInit(&result.renderer);
    ffStrbufInit(&result.vendor);
    ffStrbufInit(&result.slv);
    ffStrbufInit(&result.library);

    const char* error = ffDetectOpenGL(options, &result);
    if(error != NULL)
    {
        yyjson_mut_obj_add_str(doc, module, "error", error);
    }
    else
    {
        yyjson_mut_val* obj = yyjson_mut_obj_add_obj(doc, module, "result");
        yyjson_mut_obj_add_strbuf(doc, obj, "version", &result.version);
        yyjson_mut_obj_add_strbuf(doc, obj, "renderer", &result.renderer);
        yyjson_mut_obj_add_strbuf(doc, obj, "vendor", &result.vendor);
        yyjson_mut_obj_add_strbuf(doc, obj, "slv", &result.slv);
        yyjson_mut_obj_add_strbuf(doc, obj, "library", &result.library);
    }

    ffStrbufDestroy(&result.version);
    ffStrbufDestroy(&result.renderer);
    ffStrbufDestroy(&result.vendor);
    ffStrbufDestroy(&result.slv);
    ffStrbufDestroy(&result.library);
}

static FFModuleBaseInfo ffModuleInfo = {
    .name = FF_OPENGL_MODULE_NAME,
    .description = "Print highest OpenGL version supported by the GPU",
    .parseCommandOptions = (void*) ffParseOpenGLCommandOptions,
    .parseJsonObject = (void*) ffParseOpenGLJsonObject,
    .printModule = (void*) ffPrintOpenGL,
    .generateJsonResult = (void*) ffGenerateOpenGLJsonResult,
    .generateJsonConfig = (void*) ffGenerateOpenGLJsonConfig,
    .formatArgs = FF_FORMAT_ARG_LIST(((FFModuleFormatArg[]) {
        {"OpenGL version", "version"},
        {"OpenGL renderer", "renderer"},
        {"OpenGL vendor", "vendor"},
        {"OpenGL shading language version", "slv"},
        {"OpenGL library used", "library"},
    }))
};

void ffInitOpenGLOptions(FFOpenGLOptions* options)
{
    options->moduleInfo = ffModuleInfo;
    ffOptionInitModuleArg(&options->moduleArgs, "");

    options->library = FF_OPENGL_LIBRARY_AUTO;
}

void ffDestroyOpenGLOptions(FFOpenGLOptions* options)
{
    ffOptionDestroyModuleArg(&options->moduleArgs);
}
