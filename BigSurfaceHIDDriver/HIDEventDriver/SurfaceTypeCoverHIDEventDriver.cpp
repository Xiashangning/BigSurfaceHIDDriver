//
//  SurfaceTypeCoverHIDEventDriver.hpp
//  SurfaceTypeCover
//
//  Created by Xavier on 21/04/2021.
//  Copyright © 2021 Xia Shangning. All rights reserved.
//

#include "SurfaceTypeCoverHIDEventDriver.hpp"
#include <IOKit/hid/AppleHIDUsageTables.h>

// constants for processing the special key input event
#define kHIDIncrVolume  0x01
#define kHIDDecrVolume  0x02
#define kHIDMute        0x04

#define super VoodooI2CPrecisionTouchpadHIDEventDriver
OSDefineMetaClassAndStructors(SurfaceTypeCoverHIDEventDriver, VoodooI2CPrecisionTouchpadHIDEventDriver);

void SurfaceTypeCoverHIDEventDriver::handleInterruptReport(AbsoluteTime timestamp, IOMemoryDescriptor *report, IOHIDReportType report_type, UInt32 report_id) {
    super::handleInterruptReport(timestamp, report, report_type, report_id);
    
    handleKeyboardReport(timestamp, report_id);
}

void SurfaceTypeCoverHIDEventDriver::handleKeyboardReport(AbsoluteTime timeStamp, UInt32 reportID) {
    UInt32 volumeHandled = 0;
    UInt32 volumeState = 0;
    UInt32 index, count;
    
    if(!keyboard.elements)
        return;
    
    for (index=0, count=keyboard.elements->getCount(); index<count; index++) {
        IOHIDElement* element = nullptr;
        AbsoluteTime  elementTimeStamp;
        UInt32        usagePage, usage, value, preValue;
        
        element = OSDynamicCast(IOHIDElement, keyboard.elements->getObject(index));
        if (!element)
            continue;
        
        if (element->getReportID() != reportID)
            continue;
        
        elementTimeStamp = element->getTimeStamp();
        if (CMP_ABSOLUTETIME(&timeStamp, &elementTimeStamp) != 0)
            continue;
        
        preValue    = element->getValue(kIOHIDValueOptionsFlagPrevious) != 0;
        value       = element->getValue() != 0;
        
        if (value == preValue)
            continue;
        
        usagePage   = element->getUsagePage();
        usage       = element->getUsage();
        
        if (usagePage == kHIDPage_Consumer) {
            bool suppress = true;
            switch (usage) {
                case kHIDUsage_Csmr_VolumeIncrement:
                    volumeHandled   |= kHIDIncrVolume;
                    volumeState     |= (value) ? kHIDIncrVolume:0;
                    break;
                case kHIDUsage_Csmr_VolumeDecrement:
                    volumeHandled   |= kHIDDecrVolume;
                    volumeState     |= (value) ? kHIDDecrVolume:0;
                    break;
                case kHIDUsage_Csmr_Mute:
                    volumeHandled   |= kHIDMute;
                    volumeState     |= (value) ? kHIDMute:0;
                    break;
                default:
                    suppress = false;
                    break;
            }
            
            if (suppress)
                continue;
        }
        
        dispatchKeyboardEvent(timeStamp, usagePage, usage, value);
    }
    
    // RY: Handle the case where Vol Increment, Decrement, and Mute are all down
    // If such an event occurs, it is likely that the device is defective,
    // and should be ignored.
    if ((volumeState != (kHIDIncrVolume|kHIDDecrVolume|kHIDMute)) &&
        (volumeHandled != (kHIDIncrVolume|kHIDDecrVolume|kHIDMute))) {
        // Volume Increment
        if (volumeHandled & kHIDIncrVolume)
            dispatchKeyboardEvent(timeStamp, kHIDPage_Consumer, kHIDUsage_Csmr_VolumeIncrement, ((volumeState & kHIDIncrVolume) != 0));
        // Volume Decrement
        if (volumeHandled & kHIDDecrVolume)
            dispatchKeyboardEvent(timeStamp, kHIDPage_Consumer, kHIDUsage_Csmr_VolumeDecrement, ((volumeState & kHIDDecrVolume) != 0));
        // Volume Mute
        if (volumeHandled & kHIDMute)
            dispatchKeyboardEvent(timeStamp, kHIDPage_Consumer, kHIDUsage_Csmr_Mute, ((volumeState & kHIDMute) != 0));
    }
}

bool SurfaceTypeCoverHIDEventDriver::handleStart(IOService* provider) {
    if (!super::handleStart(provider))
        return false;
    
    setKeyboardProperties();

    return true;
}

IOReturn SurfaceTypeCoverHIDEventDriver::parseElements() {
    IOReturn ret = super::parseElements();
    if(ret != kIOReturnSuccess)
        return ret;
    
    //Keyboard : Loop through all createMatchingElements() and parse KeyboardElements
    OSArray *elementArray = hid_interface->createMatchingElements();
    keyboard.appleVendorSupported = getProperty(kIOHIDAppleVendorSupported, gIOServicePlane);
    if (elementArray) {
        for (int i=0, count=elementArray->getCount(); i<count; i++) {
            IOHIDElement* element   = nullptr;
            element = OSDynamicCast(IOHIDElement, elementArray->getObject(i));
            if (!element)
                continue;
            
            if (element->getType() == kIOHIDElementTypeCollection)
                continue;
            
            if (element->getUsage() == 0)
                continue;
            
            parseKeyboardElement(element);
        }
    }
    OSSafeReleaseNULL(elementArray);
    
    return kIOReturnSuccess;
}

bool SurfaceTypeCoverHIDEventDriver::parseKeyboardElement(IOHIDElement * element) {
    UInt32 usagePage    = element->getUsagePage();
    UInt32 usage        = element->getUsage();
    bool   store        = false;
    
    if (!keyboard.elements) {
        keyboard.elements = OSArray::withCapacity(4);
        if(!keyboard.elements)
            return false;
    }
    
    switch (usagePage) {
        case kHIDPage_GenericDesktop:
            switch (usage) {
                case kHIDUsage_GD_Start:
                case kHIDUsage_GD_Select:
                case kHIDUsage_GD_SystemPowerDown:
                case kHIDUsage_GD_SystemSleep:
                case kHIDUsage_GD_SystemWakeUp:
                case kHIDUsage_GD_SystemContextMenu:
                case kHIDUsage_GD_SystemMainMenu:
                case kHIDUsage_GD_SystemAppMenu:
                case kHIDUsage_GD_SystemMenuHelp:
                case kHIDUsage_GD_SystemMenuExit:
                case kHIDUsage_GD_SystemMenuSelect:
                case kHIDUsage_GD_SystemMenuRight:
                case kHIDUsage_GD_SystemMenuLeft:
                case kHIDUsage_GD_SystemMenuUp:
                case kHIDUsage_GD_SystemMenuDown:
                case kHIDUsage_GD_DPadUp:
                case kHIDUsage_GD_DPadDown:
                case kHIDUsage_GD_DPadRight:
                case kHIDUsage_GD_DPadLeft:
                    store = true;
                    break;
            }
            break;
        case kHIDPage_KeyboardOrKeypad:
            if ((usage < kHIDUsage_KeyboardA) || (usage > kHIDUsage_KeyboardRightGUI))
                break;
            
            store = true;
            break;
        case kHIDPage_Consumer:
            if (usage == kHIDUsage_Csmr_ACKeyboardLayoutSelect)
                setProperty(kIOHIDSupportsGlobeKeyKey, kOSBooleanTrue);
        case kHIDPage_Telephony:
            store = true;
            break;
        case kHIDPage_AppleVendorTopCase:
            if (keyboard.appleVendorSupported) {
                switch (usage) {
                    case kHIDUsage_AV_TopCase_BrightnessDown:
                    case kHIDUsage_AV_TopCase_BrightnessUp:
                    case kHIDUsage_AV_TopCase_IlluminationDown:
                    case kHIDUsage_AV_TopCase_IlluminationUp:
                    case kHIDUsage_AV_TopCase_KeyboardFn:
                        store = true;
                        break;
                }
            }
            break;
        case kHIDPage_AppleVendorKeyboard:
            if (keyboard.appleVendorSupported) {
                switch (usage) {
                    case kHIDUsage_AppleVendorKeyboard_Spotlight:
                    case kHIDUsage_AppleVendorKeyboard_Dashboard:
                    case kHIDUsage_AppleVendorKeyboard_Function:
                    case kHIDUsage_AppleVendorKeyboard_Launchpad:
                    case kHIDUsage_AppleVendorKeyboard_Reserved:
                    case kHIDUsage_AppleVendorKeyboard_CapsLockDelayEnable:
                    case kHIDUsage_AppleVendorKeyboard_PowerState:
                    case kHIDUsage_AppleVendorKeyboard_Expose_All:
                    case kHIDUsage_AppleVendorKeyboard_Expose_Desktop:
                    case kHIDUsage_AppleVendorKeyboard_Brightness_Up:
                    case kHIDUsage_AppleVendorKeyboard_Brightness_Down:
                    case kHIDUsage_AppleVendorKeyboard_Language:
                        store = true;
                        break;
                }
            }
            break;
    }
    
    if(store)
        keyboard.elements->setObject(element);
    
    return store;
}

void SurfaceTypeCoverHIDEventDriver::setKeyboardProperties()
{
    OSDictionary *properties = OSDictionary::withCapacity(4);
    if (!properties)
        return;
    
    if (!keyboard.elements) {
        OSSafeReleaseNULL(properties);
        return;
    }
    
    properties->setObject(kIOHIDElementKey, keyboard.elements);
    setProperty("Keyboard", properties);
    
    OSSafeReleaseNULL(properties);
}
