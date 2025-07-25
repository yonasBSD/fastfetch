#include "common/printing.h"
#include "common/jsonconfig.h"
#include "detection/media/media.h"
#include "modules/player/player.h"
#include "util/stringUtils.h"

#include <ctype.h>

#define FF_PLAYER_DISPLAY_NAME "Media Player"

void ffPrintPlayer(FFPlayerOptions* options)
{
    const FFMediaResult* media = ffDetectMedia();

    if(media->error.length > 0)
    {
        ffPrintError(FF_PLAYER_DISPLAY_NAME, 0, &options->moduleArgs, FF_PRINT_TYPE_DEFAULT, "%s", media->error.chars);
        return;
    }

    if (media->player.length == 0)
    {
        ffPrintError(FF_PLAYER_DISPLAY_NAME, 0, &options->moduleArgs, FF_PRINT_TYPE_DEFAULT, "No media player detected");
        return;
    }

    FF_STRBUF_AUTO_DESTROY playerPretty = ffStrbufCreate();

    if (media->url.length > 0)
    {
        //If we are on a website, prepend the website name
        if(
            ffStrbufIgnCaseEqualS(&media->playerId, "spotify") ||
            ffStrbufIgnCaseEqualS(&media->playerId, "vlc")
        ) {} // do noting, surely not a website, even if the url is set
        else if(ffStrbufStartsWithS(&media->url, "https://www."))
            ffStrbufAppendS(&playerPretty, media->url.chars + 12);
        else if(ffStrbufStartsWithS(&media->url, "http://www."))
            ffStrbufAppendS(&playerPretty, media->url.chars + 11);
        else if(ffStrbufStartsWithS(&media->url, "https://"))
            ffStrbufAppendS(&playerPretty, media->url.chars + 8);
        else if(ffStrbufStartsWithS(&media->url, "http://"))
            ffStrbufAppendS(&playerPretty, media->url.chars + 7);
    }

    //If we found a website name, make it more pretty
    if(playerPretty.length > 0)
    {
        ffStrbufSubstrBeforeFirstC(&playerPretty, '/'); //Remove the path
        ffStrbufSubstrBeforeLastC(&playerPretty, '.'); //Remove the TLD
    }

    //Check again for length, as we may have removed everything.
    bool playerPrettyIsCustom = playerPretty.length > 0;

    //If we don't have subdomains, it is usually more pretty to capitalize the first letter.
    if(playerPrettyIsCustom && ffStrbufFirstIndexC(&playerPretty, '.') == playerPretty.length)
        playerPretty.chars[0] = (char) toupper(playerPretty.chars[0]);

    if(playerPrettyIsCustom)
        ffStrbufAppendS(&playerPretty, " (");

    ffStrbufAppend(&playerPretty, &media->player);

    if(playerPrettyIsCustom)
        ffStrbufAppendC(&playerPretty, ')');

    if(options->moduleArgs.outputFormat.length == 0)
    {
        ffPrintLogoAndKey(FF_PLAYER_DISPLAY_NAME, 0, &options->moduleArgs, FF_PRINT_TYPE_DEFAULT);
        ffStrbufPutTo(&playerPretty, stdout);
    }
    else
    {
        FF_PRINT_FORMAT_CHECKED(FF_PLAYER_DISPLAY_NAME, 0, &options->moduleArgs, FF_PRINT_TYPE_DEFAULT, ((FFformatarg[]){
            FF_FORMAT_ARG(playerPretty, "player"),
            FF_FORMAT_ARG(media->player, "name"),
            FF_FORMAT_ARG(media->playerId, "id"),
            FF_FORMAT_ARG(media->url, "url"),
        }));
    }
}

bool ffParsePlayerCommandOptions(FFPlayerOptions* options, const char* key, const char* value)
{
    const char* subKey = ffOptionTestPrefix(key, FF_PLAYER_MODULE_NAME);
    if (!subKey) return false;
    if (ffOptionParseModuleArgs(key, subKey, value, &options->moduleArgs))
        return true;

    return false;
}

void ffParsePlayerJsonObject(FFPlayerOptions* options, yyjson_val* module)
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

        ffPrintError(FF_PLAYER_MODULE_NAME, 0, &options->moduleArgs, FF_PRINT_TYPE_DEFAULT, "Unknown JSON key %s", key);
    }
}

void ffGeneratePlayerJsonConfig(FFPlayerOptions* options, yyjson_mut_doc* doc, yyjson_mut_val* module)
{
    __attribute__((__cleanup__(ffDestroyPlayerOptions))) FFPlayerOptions defaultOptions;
    ffInitPlayerOptions(&defaultOptions);

    ffJsonConfigGenerateModuleArgsConfig(doc, module, &defaultOptions.moduleArgs, &options->moduleArgs);
}

void ffGeneratePlayerJsonResult(FF_MAYBE_UNUSED FFMediaOptions* options, yyjson_mut_doc* doc, yyjson_mut_val* module)
{
    yyjson_mut_obj_add_str(doc, module, "error", "Player module is an alias of Media module");
}

static FFModuleBaseInfo ffModuleInfo = {
    .name = FF_PLAYER_MODULE_NAME,
    .description = "Print music player name",
    .parseCommandOptions = (void*) ffParsePlayerCommandOptions,
    .parseJsonObject = (void*) ffParsePlayerJsonObject,
    .printModule = (void*) ffPrintPlayer,
    .generateJsonResult = (void*) ffGeneratePlayerJsonResult,
    .generateJsonConfig = (void*) ffGeneratePlayerJsonConfig,
    .formatArgs = FF_FORMAT_ARG_LIST(((FFModuleFormatArg[]) {
        {"Pretty player name", "player"},
        {"Player name", "name"},
        {"Player Identifier", "id"},
        {"URL name", "url"},
    }))
};

void ffInitPlayerOptions(FFPlayerOptions* options)
{
    options->moduleInfo = ffModuleInfo;
    ffOptionInitModuleArg(&options->moduleArgs, "󰥠");
}

void ffDestroyPlayerOptions(FFPlayerOptions* options)
{
    ffOptionDestroyModuleArg(&options->moduleArgs);
}
