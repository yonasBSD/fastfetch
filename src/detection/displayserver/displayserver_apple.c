#include "displayserver.h"
#include "util/apple/cf_helpers.h"
#include "util/stringUtils.h"
#include "util/edidHelper.h"
#include "detection/os/os.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <CoreGraphics/CGDirectDisplay.h>
#include <CoreVideo/CVDisplayLink.h>

#ifdef MAC_OS_X_VERSION_10_15
extern Boolean CoreDisplay_Display_SupportsHDRMode(CGDirectDisplayID display) __attribute__((weak_import));
extern Boolean CoreDisplay_Display_IsHDRModeEnabled(CGDirectDisplayID display) __attribute__((weak_import));
extern CFDictionaryRef CoreDisplay_DisplayCreateInfoDictionary(CGDirectDisplayID display) __attribute__((weak_import));
#else
#include <IOKit/graphics/IOGraphicsLib.h>
#endif

static void detectDisplays(FFDisplayServerResult* ds)
{
    CGDirectDisplayID screens[128];
    uint32_t screenCount;
    if(CGGetOnlineDisplayList(ARRAY_SIZE(screens), screens, &screenCount) != kCGErrorSuccess)
        return;

    FF_STRBUF_AUTO_DESTROY buffer = ffStrbufCreate();
    for(uint32_t i = 0; i < screenCount; i++)
    {
        CGDirectDisplayID screen = screens[i];
        CGDisplayModeRef mode = CGDisplayCopyDisplayMode(screen);
        if(mode)
        {
            //https://github.com/glfw/glfw/commit/aab08712dd8142b642e2042e7b7ba563acd07a45
            double refreshRate = CGDisplayModeGetRefreshRate(mode);

            if (refreshRate == 0)
            {
                #pragma clang diagnostic push
                #pragma clang diagnostic ignored "-Wdeprecated-declarations"
                CVDisplayLinkRef link;
                if(CVDisplayLinkCreateWithCGDisplay(screen, &link) == kCVReturnSuccess)
                {
                    const CVTime time = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(link);
                    if (!(time.flags & kCVTimeIsIndefinite))
                        refreshRate = time.timeScale / (double) time.timeValue; //59.97...
                    CVDisplayLinkRelease(link);
                }
                #pragma clang diagnostic pop
            }

            ffStrbufClear(&buffer);
            CFDictionaryRef FF_CFTYPE_AUTO_RELEASE displayInfo = NULL;
            #ifdef MAC_OS_X_VERSION_10_15
            if(CoreDisplay_DisplayCreateInfoDictionary)
                displayInfo = CoreDisplay_DisplayCreateInfoDictionary(screen);
            #else
            {
                io_service_t servicePort = CGDisplayIOServicePort(screen);
                displayInfo = IODisplayCreateInfoDictionary(servicePort, kIODisplayOnlyPreferredName);
            }
            #endif

            uint32_t physicalWidth = 0, physicalHeight = 0;
            uint32_t preferredWidth = 0, preferredHeight = 0;
            double preferredRefreshRate = 0;

            if(displayInfo)
            {
                CFDictionaryRef productNames;
                if(ffCfDictGetDict(displayInfo, CFSTR(kDisplayProductName), &productNames) == NULL)
                    ffCfDictGetString(productNames, CFSTR("en_US"), &buffer);

                // CGDisplayScreenSize reports invalid result for external displays on old Intel MacBook Pro
                CFDataRef edidRef = (CFDataRef) CFDictionaryGetValue(displayInfo, CFSTR(kIODisplayEDIDKey));
                if (edidRef && CFGetTypeID(edidRef) == CFDataGetTypeID())
                {
                    const uint8_t* edidData = CFDataGetBytePtr(edidRef);
                    uint32_t edidLength = (uint32_t) CFDataGetLength(edidRef);
                    if (edidLength >= 128)
                        ffEdidGetPhysicalSize(edidData, &physicalWidth, &physicalHeight);
                }

                if (!physicalWidth || !physicalHeight)
                {
                    if (ffCfDictGetInt(displayInfo, CFSTR(kDisplayHorizontalImageSize), (int*) &physicalWidth) == NULL)
                        ffCfDictGetInt(displayInfo, CFSTR(kDisplayVerticalImageSize), (int*) &physicalHeight);
                }

                ffCfDictGetInt(displayInfo, CFSTR("kCGDisplayPixelWidth"), (int*) &preferredWidth);
                ffCfDictGetInt(displayInfo, CFSTR("kCGDisplayPixelHeight"), (int*) &preferredHeight);
                if (preferredWidth && preferredHeight)
                {
                    FF_CFTYPE_AUTO_RELEASE CFArrayRef allModes = CGDisplayCopyAllDisplayModes(screen, NULL);
                    if (allModes)
                    {
                        for (CFIndex i = 0, count = CFArrayGetCount(allModes); i < count; i++)
                        {
                            CGDisplayModeRef modeInfo = (CGDisplayModeRef) CFArrayGetValueAtIndex(allModes, i);
                            if (CGDisplayModeGetPixelWidth(modeInfo) == preferredWidth && CGDisplayModeGetPixelHeight(modeInfo) == preferredHeight)
                            {
                                double rr = CGDisplayModeGetRefreshRate(modeInfo);
                                if (rr > preferredRefreshRate) preferredRefreshRate = rr;
                                break;
                            }
                        }
                    }
                }
            }

            if ((!physicalWidth || !physicalHeight) && CGDisplayPrimaryDisplay(screen) == screen) // #1406
            {
                CGSize size = CGDisplayScreenSize(screen);
                physicalWidth = (uint32_t) (size.width + 0.5);
                physicalHeight = (uint32_t) (size.height + 0.5);
            }

            FFDisplayResult* display = ffdsAppendDisplay(ds,
                (uint32_t)CGDisplayModeGetPixelWidth(mode),
                (uint32_t)CGDisplayModeGetPixelHeight(mode),
                refreshRate,
                (uint32_t)CGDisplayModeGetWidth(mode),
                (uint32_t)CGDisplayModeGetHeight(mode),
                preferredWidth,
                preferredHeight,
                preferredRefreshRate,
                (uint32_t)CGDisplayRotation(screen),
                &buffer,
                CGDisplayIsBuiltin(screen) ? FF_DISPLAY_TYPE_BUILTIN : FF_DISPLAY_TYPE_EXTERNAL,
                CGDisplayIsMain(screen),
                (uint64_t)screen,
                physicalWidth,
                physicalHeight,
                "CoreGraphics"
            );
            if (display)
            {
                #ifndef MAC_OS_X_VERSION_10_11
                FF_CFTYPE_AUTO_RELEASE CFStringRef pe = CGDisplayModeCopyPixelEncoding(mode);
                if (pe) display->bitDepth = (uint8_t) (CFStringGetLength(pe) - CFStringFind(pe, CFSTR("B"), 0).location);
                #else
                // https://stackoverflow.com/a/33519316/9976392
                // Also shitty, but better than parsing `CFCopyDescription(mode)`
                CFDictionaryRef dict = (CFDictionaryRef) *((int64_t *)mode + 2);
                if (CFGetTypeID(dict) == CFDictionaryGetTypeID())
                {
                    int32_t bitDepth;
                    ffCfDictGetInt(dict, kCGDisplayBitsPerSample, &bitDepth);
                    display->bitDepth = (uint8_t) bitDepth;
                }
                #endif

                if (display->type == FF_DISPLAY_TYPE_BUILTIN && displayInfo)
                    display->hdrStatus = CFDictionaryContainsKey(displayInfo, CFSTR("ReferencePeakHDRLuminance"))
                        ? FF_DISPLAY_HDR_STATUS_SUPPORTED : FF_DISPLAY_HDR_STATUS_UNSUPPORTED;
                #ifdef MAC_OS_X_VERSION_10_15
                else if (CoreDisplay_Display_SupportsHDRMode)
                {
                    if (CoreDisplay_Display_SupportsHDRMode(screen))
                    {
                        display->hdrStatus = FF_DISPLAY_HDR_STATUS_SUPPORTED;
                        if (CoreDisplay_Display_IsHDRModeEnabled && CoreDisplay_Display_IsHDRModeEnabled(screen))
                            display->hdrStatus = FF_DISPLAY_HDR_STATUS_ENABLED;
                    }
                    else
                        display->hdrStatus = FF_DISPLAY_HDR_STATUS_UNSUPPORTED;
                }
                #endif

                display->serial = CGDisplaySerialNumber(screen);

                if (displayInfo)
                {
                    int value;
                    if (ffCfDictGetInt(displayInfo, CFSTR(kDisplayYearOfManufacture), &value) == NULL)
                        display->manufactureYear = (uint16_t) value;
                    if (ffCfDictGetInt(displayInfo, CFSTR(kDisplayWeekOfManufacture), &value) == NULL)
                        display->manufactureWeek = (uint16_t) value;
                }
            }
            CGDisplayModeRelease(mode);
        }
        CGDisplayRelease(screen);
    }
}

void ffConnectDisplayServerImpl(FFDisplayServerResult* ds)
{
    {
        FF_CFTYPE_AUTO_RELEASE CFMachPortRef port = CGWindowServerCreateServerPort();
        if (port)
        {
            ffStrbufSetStatic(&ds->wmProcessName, "WindowServer");
            ffStrbufSetStatic(&ds->wmPrettyName, "Quartz Compositor");
        }
    }

    const FFOSResult* os = ffDetectOS();

    char* str_end;
    const char* version = os->version.chars;
    unsigned long osNum = strtoul(version, &str_end, 10);
    if (str_end != version)
    {
        if (osNum > 15) { // Tahoe
            ffStrbufSetStatic(&ds->dePrettyName, "Liquid Glass");
        } else if (osNum < 10) {
            ffStrbufSetStatic(&ds->dePrettyName, "Platinum");
        } else {
            ffStrbufSetStatic(&ds->dePrettyName, "Aqua");
        }
    }

    detectDisplays(ds);
}
