#include "common/printing.h"
#include "common/jsonconfig.h"
#include "common/percent.h"
#include "common/size.h"
#include "detection/memory/memory.h"
#include "modules/memory/memory.h"
#include "util/stringUtils.h"

void ffPrintMemory(FFMemoryOptions* options)
{
    FFMemoryResult storage = {};
    const char* error = ffDetectMemory(&storage);

    if(error)
    {
        ffPrintError(FF_MEMORY_MODULE_NAME, 0, &options->moduleArgs, FF_PRINT_TYPE_DEFAULT, "%s", error);
        return;
    }

    FF_STRBUF_AUTO_DESTROY usedPretty = ffStrbufCreate();
    ffSizeAppendNum(storage.bytesUsed, &usedPretty);

    FF_STRBUF_AUTO_DESTROY totalPretty = ffStrbufCreate();
    ffSizeAppendNum(storage.bytesTotal, &totalPretty);

    double percentage = storage.bytesTotal == 0
        ? 0
        : (double) storage.bytesUsed / (double) storage.bytesTotal * 100.0;

    FFPercentageTypeFlags percentType = options->percent.type == 0 ? instance.config.display.percentType : options->percent.type;
    if(options->moduleArgs.outputFormat.length == 0)
    {
        ffPrintLogoAndKey(FF_MEMORY_MODULE_NAME, 0, &options->moduleArgs, FF_PRINT_TYPE_DEFAULT);
        if (storage.bytesTotal == 0)
            puts("Disabled");
        else
        {
            FF_STRBUF_AUTO_DESTROY str = ffStrbufCreate();

            if(percentType & FF_PERCENTAGE_TYPE_BAR_BIT)
            {
                ffPercentAppendBar(&str, percentage, options->percent, &options->moduleArgs);
                ffStrbufAppendC(&str, ' ');
            }

            if(!(percentType & FF_PERCENTAGE_TYPE_HIDE_OTHERS_BIT))
                ffStrbufAppendF(&str, "%s / %s ", usedPretty.chars, totalPretty.chars);

            if(percentType & FF_PERCENTAGE_TYPE_NUM_BIT)
                ffPercentAppendNum(&str, percentage, options->percent, str.length > 0, &options->moduleArgs);

            ffStrbufTrimRight(&str, ' ');
            ffStrbufPutTo(&str, stdout);
        }
    }
    else
    {
        FF_STRBUF_AUTO_DESTROY percentageNum = ffStrbufCreate();
        if (percentType & FF_PERCENTAGE_TYPE_NUM_BIT)
            ffPercentAppendNum(&percentageNum, percentage, options->percent, false, &options->moduleArgs);
        FF_STRBUF_AUTO_DESTROY percentageBar = ffStrbufCreate();
        if (percentType & FF_PERCENTAGE_TYPE_BAR_BIT)
            ffPercentAppendBar(&percentageBar, percentage, options->percent, &options->moduleArgs);

        FF_PRINT_FORMAT_CHECKED(FF_MEMORY_MODULE_NAME, 0, &options->moduleArgs, FF_PRINT_TYPE_DEFAULT, ((FFformatarg[]){
            FF_FORMAT_ARG(usedPretty, "used"),
            FF_FORMAT_ARG(totalPretty, "total"),
            FF_FORMAT_ARG(percentageNum, "percentage"),
            FF_FORMAT_ARG(percentageBar, "percentage-bar"),
        }));
    }
}

bool ffParseMemoryCommandOptions(FFMemoryOptions* options, const char* key, const char* value)
{
    const char* subKey = ffOptionTestPrefix(key, FF_MEMORY_MODULE_NAME);
    if (!subKey) return false;
    if (ffOptionParseModuleArgs(key, subKey, value, &options->moduleArgs))
        return true;

    if (ffPercentParseCommandOptions(key, subKey, value, &options->percent))
        return true;

    return false;
}

void ffParseMemoryJsonObject(FFMemoryOptions* options, yyjson_val* module)
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

        if (ffPercentParseJsonObject(key, val, &options->percent))
            continue;

        ffPrintError(FF_MEMORY_MODULE_NAME, 0, &options->moduleArgs, FF_PRINT_TYPE_DEFAULT, "Unknown JSON key %s", key);
    }
}

void ffGenerateMemoryJsonConfig(FFMemoryOptions* options, yyjson_mut_doc* doc, yyjson_mut_val* module)
{
    __attribute__((__cleanup__(ffDestroyMemoryOptions))) FFMemoryOptions defaultOptions;
    ffInitMemoryOptions(&defaultOptions);

    ffJsonConfigGenerateModuleArgsConfig(doc, module, &defaultOptions.moduleArgs, &options->moduleArgs);

    ffPercentGenerateJsonConfig(doc, module, defaultOptions.percent, options->percent);
}

void ffGenerateMemoryJsonResult(FF_MAYBE_UNUSED FFMemoryOptions* options, yyjson_mut_doc* doc, yyjson_mut_val* module)
{
    FFMemoryResult storage = {};
    const char* error = ffDetectMemory(&storage);

    if(error)
    {
        yyjson_mut_obj_add_str(doc, module, "error", error);
        return;
    }

    yyjson_mut_val* obj = yyjson_mut_obj_add_obj(doc, module, "result");
    yyjson_mut_obj_add_uint(doc, obj, "total", storage.bytesTotal);
    yyjson_mut_obj_add_uint(doc, obj, "used", storage.bytesUsed);
}

static FFModuleBaseInfo ffModuleInfo = {
    .name = FF_MEMORY_MODULE_NAME,
    .description = "Print system memory usage info",
    .parseCommandOptions = (void*) ffParseMemoryCommandOptions,
    .parseJsonObject = (void*) ffParseMemoryJsonObject,
    .printModule = (void*) ffPrintMemory,
    .generateJsonResult = (void*) ffGenerateMemoryJsonResult,
    .generateJsonConfig = (void*) ffGenerateMemoryJsonConfig,
    .formatArgs = FF_FORMAT_ARG_LIST(((FFModuleFormatArg[]) {
        {"Used size", "used"},
        {"Total size", "total"},
        {"Percentage used (num)", "percentage"},
        {"Percentage used (bar)", "percentage-bar"},
    }))
};

void ffInitMemoryOptions(FFMemoryOptions* options)
{
    options->moduleInfo = ffModuleInfo;
    ffOptionInitModuleArg(&options->moduleArgs, "");
    options->percent = (FFPercentageModuleConfig) { 50, 80, 0 };
}

void ffDestroyMemoryOptions(FFMemoryOptions* options)
{
    ffOptionDestroyModuleArg(&options->moduleArgs);
}
