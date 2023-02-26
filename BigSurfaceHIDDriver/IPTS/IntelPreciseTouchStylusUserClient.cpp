//
//  IntelPreciseTouchStylusUserClient.cpp
//  SurfaceTouchScreen
//
//  Created by Xavier on 2022/6/11.
//  Copyright © 2022 Xia Shangning. All rights reserved.
//

#include "IntelPreciseTouchStylusUserClient.hpp"

#define super IOUserClient
OSDefineMetaClassAndStructors(IntelPreciseTouchStylusUserClient, IOUserClient);

const IOExternalMethodDispatch IntelPreciseTouchStylusUserClient::methods[kNumberOfMethods] = {
    [kMethodGetDeviceInfo] = {
        .function = (IOExternalMethodAction)&IntelPreciseTouchStylusUserClient::sMethodGetDeviceInfo,
        .checkScalarInputCount = 0,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = sizeof(IPTSDeviceInfo),
    },
    [kMethodReceiveInput] = {
        .function = (IOExternalMethodAction)&IntelPreciseTouchStylusUserClient::sMethodReceiveInput,
        .checkScalarInputCount = 0,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 1,
        .checkStructureOutputSize = 0,
    },
    [kMethodSendHIDReport] = {
        .function = (IOExternalMethodAction)&IntelPreciseTouchStylusUserClient::sMethodSendHIDReport,
        .checkScalarInputCount = 0,
        .checkStructureInputSize = sizeof(IPTSHIDReport),
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    [kMethodToggleProcessingStatus] = {
        .function = (IOExternalMethodAction)&IntelPreciseTouchStylusUserClient::sMethodToggleProcessingStatus,
        .checkScalarInputCount = 1,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
};

IOReturn IntelPreciseTouchStylusUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *arguments, IOExternalMethodDispatch *dispatch, OSObject *target, void *reference) {
    if (selector < kNumberOfMethods) {
        dispatch = const_cast<IOExternalMethodDispatch *>(&methods[selector]);
        if (!target)
            target = this;
    }
    return super::externalMethod(selector, arguments, dispatch, target, reference);
}

IOReturn IntelPreciseTouchStylusUserClient::clientMemoryForType(UInt32 type, IOOptionBits *options, IOMemoryDescriptor **memory) {
    IOMemoryDescriptor *desc = driver->getReceiveBuffer();
    if (!desc)
        return kIOReturnError;
    
    *memory = desc;
    *options |= kIOMapReadOnly;
    return kIOReturnSuccess;
}

bool IntelPreciseTouchStylusUserClient::initWithTask(task_t owningTask, void *securityToken, UInt32 type, OSDictionary *properties) {
    if (!owningTask)
        return false;
    if (!super::initWithTask(owningTask, securityToken, type))
        return false;
    
    task = owningTask;
    return true;
}

bool IntelPreciseTouchStylusUserClient::start(IOService *provider) {
    if (!super::start(provider))
        return false;
    
    driver = OSDynamicCast(IntelPreciseTouchStylusDriver, provider);
    if (!driver)
        return false;
    
    return true;
}

void IntelPreciseTouchStylusUserClient::stop(IOService* provider) {
    driver->exitMultitouch();
    super::stop(provider);
}

IOReturn IntelPreciseTouchStylusUserClient::clientClose(void) {
    if (!isInactive())
        terminate();
    return kIOReturnSuccess;
}

IOReturn IntelPreciseTouchStylusUserClient::sMethodGetDeviceInfo(OSObject *target, void *ref, IOExternalMethodArguments *args) {
    IntelPreciseTouchStylusUserClient *that = OSDynamicCast(IntelPreciseTouchStylusUserClient, target);
    if (!that)
        return kIOReturnError;
    return that->getDeviceInfo(ref, args);
}

IOReturn IntelPreciseTouchStylusUserClient::getDeviceInfo(void *ref, IOExternalMethodArguments *args) {
    return driver->getDeviceInfo(reinterpret_cast<IPTSDeviceInfo *>(args->structureOutput));
}

IOReturn IntelPreciseTouchStylusUserClient::sMethodReceiveInput(OSObject *target, void *ref, IOExternalMethodArguments *args) {
    IntelPreciseTouchStylusUserClient *that = OSDynamicCast(IntelPreciseTouchStylusUserClient, target);
    if (!that)
        return kIOReturnError;
    return that->receiveInput(ref, args);
}

IOReturn IntelPreciseTouchStylusUserClient::receiveInput(void *ref, IOExternalMethodArguments *args) {
    if (initial) {
        driver->enterMultitouch();
        initial = false;
    }
    return driver->waitInput(args->scalarOutput);
}

IOReturn IntelPreciseTouchStylusUserClient::sMethodSendHIDReport(OSObject *target, void *ref, IOExternalMethodArguments *args) {
    IntelPreciseTouchStylusUserClient *that = OSDynamicCast(IntelPreciseTouchStylusUserClient, target);
    if (!that)
        return kIOReturnError;
    return that->sendHIDReport(ref, args);
}

IOReturn IntelPreciseTouchStylusUserClient::sendHIDReport(void *ref, IOExternalMethodArguments *args) {
    driver->handleHIDReport(reinterpret_cast<const IPTSHIDReport *>(args->structureInput));
    return kIOReturnSuccess;
}

IOReturn IntelPreciseTouchStylusUserClient::sMethodToggleProcessingStatus(OSObject *target, void *ref, IOExternalMethodArguments *args) {
    IntelPreciseTouchStylusUserClient *that = OSDynamicCast(IntelPreciseTouchStylusUserClient, target);
    if (!that)
        return kIOReturnError;
    return that->toggleProcessingStatus(ref, args);
}

IOReturn IntelPreciseTouchStylusUserClient::toggleProcessingStatus(void *ref, IOExternalMethodArguments *args) {
    if (args->scalarInput[0])
        driver->processingStarted();
    else
        driver->processingEnded();
    return kIOReturnSuccess;
}
