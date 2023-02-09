//
//  IntelPreciseTouchStylusDriver.cpp
//  SurfaceTouchScreen
//
//  Created by Xavier on 2022/6/5.
//  Copyright © 2022 Xia Shangning. All rights reserved.
//

#include "IntelPreciseTouchStylusDriver.hpp"
#include "SurfaceTouchScreenDevice.hpp"

#define LOG(str, ...)    IOLog("%s::" str "\n", "IntelPreciseTouchStylusDriver", ##__VA_ARGS__)

#define super IOService
OSDefineMetaClassAndStructors(IntelPreciseTouchStylusDriver, IOService)

IOService* IntelPreciseTouchStylusDriver::probe(IOService* provider, SInt32* score) {
    if (!super::probe(provider, score))
        return nullptr;

    api = OSDynamicCast(SurfaceManagementEngineClient, provider);
    if (!api)
        return nullptr;

    return this;
}

bool IntelPreciseTouchStylusDriver::start(IOService *provider) {
    if (!super::start(provider))
        return false;
    
    input_lock = IOLockAlloc();
    if (!input_lock) {
        LOG("Failed to create lock");
        return false;
    }
    
    work_loop = IOWorkLoop::workLoop();
    if (!work_loop) {
        LOG("Failed to create work loop");
        return false;
    }
    
    command_gate = IOCommandGate::commandGate(this);
    if (!command_gate) {
        LOG("Failed to create command gate");
        goto exit;
    }
    work_loop->addEventSource(command_gate);
    
    wait_input = OSMemberFunctionCast(IOCommandGate::Action, this, &IntelPreciseTouchStylusDriver::waitInputGated);
    handle_report = OSMemberFunctionCast(IOCommandGate::Action, this, &IntelPreciseTouchStylusDriver::handleHIDReportGated);
    interrupt_source = IOInterruptEventSource::interruptEventSource(this, OSMemberFunctionCast(IOInterruptEventAction, this, &IntelPreciseTouchStylusDriver::handleInterruptReport));
    if (!interrupt_source) {
        LOG("Failed to create interrupt source!");
        goto exit;
    }
    work_loop->addEventSource(interrupt_source);
    
    timer = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &IntelPreciseTouchStylusDriver::pollTouchData));
    if (!timer) {
        LOG("Failed to create timer");
        goto exit;
    }
    work_loop->addEventSource(timer);
    
    // Publishing touch screen device
    touch_screen = OSTypeAlloc(SurfaceTouchScreenDevice);
    if (!touch_screen || !touch_screen->init() || !touch_screen->attach(this)) {
        LOG("Failed to init Surface Touch Screen device");
        goto exit;
    }
    
    report_to_send = IOBufferMemoryDescriptor::withCapacity(sizeof(IPTSHIDReport), kIODirectionNone);
    if (!report_to_send) {
        LOG("Failed to create report buffer");
        goto exit;
    }
    
    if (api->registerMessageHandler(this, OSMemberFunctionCast(SurfaceManagementEngineClient::MessageHandler, this, &IntelPreciseTouchStylusDriver::handleMessage)) != kIOReturnSuccess) {
        LOG("Failed to register receive handler");
        goto exit;
    }
    
    if (startDevice() != kIOReturnSuccess) {
        LOG("Failed to start IPTS device!");
        goto exit;
    }
    
    PMinit();
    api->joinPMtree(this);
    registerPowerDriver(this, MyIOPMPowerStates, kIOPMNumberPowerStates);
    
    registerService();
    return true;
exit:
    releaseResources();
    return false;
}

void IntelPreciseTouchStylusDriver::stop(IOService *provider) {
    stopDevice();
    // Wait at most 500ms for device to stop
    for (int i = 0; i < 20; i++) {
        if (state == IPTSDeviceStateStopped)
            break;
        IOSleep(25);
    }
    PMstop();
    releaseResources();
    super::stop(provider);
}

IOReturn IntelPreciseTouchStylusDriver::setPowerState(unsigned long whichState, IOService *whatDevice) {
    if (whatDevice != this)
        return kIOReturnInvalid;
    if (whichState == 0) {
        if (awake) {
            awake = false;
            stopDevice();
            // Wait at most 500ms for device to stop
            for (int i = 0; i < 20; i++) {
                if (state == IPTSDeviceStateStopped)
                    break;
                IOSleep(25);
            }
            current_doorbell = 0;
            LOG("Going to sleep");
        }
    } else {
        if (!awake) {
            command_gate->commandWakeup(&wait);
            IOReturn ret = kIOReturnSuccess;
            for (int i = 0; i < 3; i++) {
                IOSleep(100);
                ret = startDevice();
                if (ret != kIOReturnNoDevice || i == 2)
                    break;
                IOSleep(400);
            }
            if (ret != kIOReturnSuccess)
                LOG("Failed to restart IPTS device from sleep!");
            awake = true;
            LOG("Woke up");
        }
    }
    return kIOPMAckImplied;
}

void IntelPreciseTouchStylusDriver::releaseResources() {
    api->unregisterMessageHandler(this);
    if (timer) {
        timer->cancelTimeout();
        timer->disable();
        work_loop->removeEventSource(timer);
        OSSafeReleaseNULL(timer);
    }
    if (interrupt_source) {
        interrupt_source->disable();
        work_loop->removeEventSource(interrupt_source);
        OSSafeReleaseNULL(interrupt_source);
    }
    if (command_gate) {
        command_gate->disable();
        work_loop->removeEventSource(command_gate);
        OSSafeReleaseNULL(command_gate);
    }
    OSSafeReleaseNULL(work_loop);
    
    IOLockFree(input_lock);
    
    if (touch_screen) {
        touch_screen->stop(this);
        touch_screen->detach(this);
        OSSafeReleaseNULL(touch_screen);
    }
}

IOBufferMemoryDescriptor *IntelPreciseTouchStylusDriver::getReceiveBuffer() {
    input_buffer->retain();
    return input_buffer;
}

UInt16 IntelPreciseTouchStylusDriver::getVendorID() {
    return touch_screen->vendor_id;
}

UInt16 IntelPreciseTouchStylusDriver::getDeviceID() {
    return touch_screen->device_id;
}

UInt8 IntelPreciseTouchStylusDriver::getMaxContacts() {
    return touch_screen->max_contacts;
}

IOReturn IntelPreciseTouchStylusDriver::waitInput() {
    return command_gate->runAction(wait_input);
}

IOReturn IntelPreciseTouchStylusDriver::waitInputGated() {
    IOReturn ret = command_gate->commandSleep(&wait);
    
    if (ret != THREAD_AWAKENED)
        return kIOReturnError;
    
    if (!awake)
        return kIOReturnAborted;
    else
        return kIOReturnSuccess;
}

void IntelPreciseTouchStylusDriver::processingStarted() {
    IOTakeLock(input_lock);
}

void IntelPreciseTouchStylusDriver::processingEnded() {
    IOUnlock(input_lock);
}

void IntelPreciseTouchStylusDriver::handleInterruptReport(IOInterruptEventSource *sender, int count) {
    if (sent)
        return;
    
    touch_screen->handleReport(report_to_send);
    sent = true;
}

IOReturn IntelPreciseTouchStylusDriver::handleHIDReportGated(const IPTSHIDReport *report) {
    UInt32 report_size = 0;
    switch (report->report_id) {
        case IPTS_TOUCH_REPORT_ID:
            report_size = sizeof(IPTSTouchHIDReport)+1;
            break;
        case IPTS_STYLUS_REPORT_ID:
            report_size = sizeof(IPTSStylusHIDReport)+1;
            break;
        default:
            LOG("Unknown report received! 0x%x", report->report_id);
            return kIOReturnInvalid;
    }
    report_to_send->setLength(report_size);
    report_to_send->writeBytes(0, report, report_size);
    sent = false;
    interrupt_source->interruptOccurred(nullptr, this, 0);
    return kIOReturnSuccess;
}

void IntelPreciseTouchStylusDriver::handleHIDReport(const IPTSHIDReport *report) {
    command_gate->runAction(handle_report);
}

void IntelPreciseTouchStylusDriver::pollTouchData(IOTimerEventSource *sender) {
    UInt32 doorbell;
    memcpy(&doorbell, doorbell_buffer.vaddr, sizeof(UInt32));
    
    if (doorbell == current_doorbell) {
        if (!busy) {
            timer->setTimeoutMS(IPTS_IDLE_TIMEOUT);
            return;
        }
        AbsoluteTime cur_time;
        UInt64 nsecs;
        clock_get_uptime(&cur_time);
        SUB_ABSOLUTETIME(&cur_time, &last_activate);
        absolutetime_to_nanoseconds(cur_time, &nsecs);
        if (nsecs < 500000000) // < 500ms
            timer->setTimeoutMS(IPTS_BUSY_TIMEOUT);
        else if (nsecs < 1500000000) // < 1.5s
            timer->setTimeoutMS(IPTS_ACTIVE_TIMEOUT);
        else {
            busy = false;
            timer->setTimeoutMS(IPTS_IDLE_TIMEOUT);
        }
    } else if (doorbell < current_doorbell) {  // MEI device has been reset
        LOG("MEI device has reset! Flushing buffers...");
        for (int i = 0; i < IPTS_BUFFER_NUM; i++)
            refillBuffer(i, false);     // non blocking feedback
        current_doorbell = doorbell;
        timer->setTimeoutMS(IPTS_BUSY_TIMEOUT);
    } else {
        UInt32 buffer = current_doorbell % IPTS_BUFFER_NUM;
        IPTSDataHeader *header = reinterpret_cast<IPTSDataHeader *>(rx_buffer[buffer].vaddr);
        if (header->size != 0) {
            switch (header->type) {
                case IPTSDataTypeFrame:
                    // fake a hid report
                    if (IOTryLock(input_lock)) {
                        UInt8 *temp = reinterpret_cast<UInt8 *>(input_buffer->getBytesNoCopy());
                        IPTSHIDHeader *h = reinterpret_cast<IPTSHIDHeader *>(temp+3);
                        h->type = IPTS_HID_FRAME_TYPE_RAW;
                        h->size = header->size + sizeof(IPTSHIDHeader);
                        input_buffer->writeBytes(10, header->data, header->size);
                        IOUnlock(input_lock);
                        command_gate->commandWakeup(&wait);
                    }
                    break;
                case IPTSDataTypeHID:
                    if (header->data[0] == IPTS_TOUCH_REPORT_ID) {    // directly send the single touch report
                        report_to_send->setLength(sizeof(IPTSTouchHIDReport)+1);
                        IPTSHIDReport *buffer = reinterpret_cast<IPTSHIDReport *>(report_to_send->getBytesNoCopy());
                        memset(buffer, 0, report_to_send->getLength());
                        report_to_send->writeBytes(0, header->data, header->size);
                        buffer->report.touch.contact_num = buffer->report.touch.fingers[0].touch;
                        sent = false;
                        interrupt_source->interruptOccurred(nullptr, this, 0);
                    } else {    // call the userspace daemon to process data
                        if (IOTryLock(input_lock)) {    // if the userspace daemon has finished processing
                            input_buffer->writeBytes(0, header->data, header->size);
                            IOUnlock(input_lock);
                            command_gate->commandWakeup(&wait);
                        }
                    }
                    break;
                case IPTSDataTypeGetFeatures:
                    // response of a get feature report command
                default:
                    LOG("Got data with type %d", header->type);
                    break;
            }
        }
        
        if (refillBuffer(buffer, false) != kIOReturnSuccess)
            LOG("Failed to send feedback buffer");
        
        current_doorbell++;
        busy = true;
        clock_get_uptime(&last_activate);
        timer->setTimeoutMS(IPTS_BUSY_TIMEOUT);
    }
}

IOReturn IntelPreciseTouchStylusDriver::sendIPTSCommand(UInt32 code, UInt8 *data, UInt16 data_len, bool blocking) {
    IPTSCommand cmd;

    memset(&cmd, 0, sizeof(IPTSCommand));
    cmd.code = code;

    if (data && data_len > 0)
        memcpy(&cmd.payload, data, data_len);

    IOReturn ret = api->sendMessage(reinterpret_cast<UInt8 *>(&cmd), sizeof(cmd.code) + data_len, blocking);
    if (ret != kIOReturnSuccess && (ret != kIOReturnNoDevice || state != IPTSDeviceStateStopping))
        LOG("Error while sending: 0x%X:%d", code, ret);
    
    return ret;
}

IOReturn IntelPreciseTouchStylusDriver::sendFeedback(UInt32 buffer, bool blocking) {
    IPTSFeedbackCommand cmd;

    memset(&cmd, 0, sizeof(IPTSFeedbackCommand));
    cmd.buffer = buffer;

    return sendIPTSCommand(IPTS_CMD_FEEDBACK, reinterpret_cast<UInt8 *>(&cmd), sizeof(IPTSFeedbackCommand), blocking);
}

IOReturn IntelPreciseTouchStylusDriver::sendSetFeatureReport(UInt8 report_id, UInt8 value) {
    IPTSFeedbackHeader *feedback;
    memset(tx_buffer.vaddr, 0, tx_buffer.len);
    
    feedback = reinterpret_cast<IPTSFeedbackHeader *>(tx_buffer.vaddr);

    feedback->cmd_type = IPTSFeedbackCommandTypeNone;
    feedback->data_type = IPTSFeedbackDataTypeSetFeatures;
    feedback->buffer = IPTS_TX_BUFFER;
    feedback->size = 2;
    feedback->payload[0] = report_id;
    feedback->payload[1] = value;

    return sendFeedback(IPTS_TX_BUFFER);
}

IOReturn IntelPreciseTouchStylusDriver::refillBuffer(UInt32 buffer, bool blocking) {
//    memset(feedback_buffer[buffer].vaddr, 0, feedback_buffer[buffer].len);
//    IPTSFeedbackHeader *header = reinterpret_cast<IPTSFeedbackHeader *>(feedback_buffer[buffer].vaddr);
//    header->buffer = buffer;
    
    return sendFeedback(buffer, blocking);
}

IOReturn IntelPreciseTouchStylusDriver::startDevice() {
    if (state != IPTSDeviceStateStopped)
        return kIOReturnBusy;
    
    state = IPTSDeviceStateStarting;
    restart = false;
    
    return sendIPTSCommand(IPTS_CMD_GET_DEVICE_INFO, nullptr, 0);
}

void IntelPreciseTouchStylusDriver::stopDevice() {
    if (state == IPTSDeviceStateStopping || state == IPTSDeviceStateStopped)
        return;
    state = IPTSDeviceStateStopping;
    
    if (mode == IPTSModeDoorbell) {
        timer->cancelTimeout();
        timer->disable();
    }
       
    if (sendFeedback(0) == kIOReturnNoDevice)
        state = IPTSDeviceStateStopped;
}

void IntelPreciseTouchStylusDriver::restartDevice() {
    if (restart)
        return;
    restart = true;
    stopDevice();
}

void IntelPreciseTouchStylusDriver::handleMessage(SurfaceManagementEngineClient *sender, UInt8 *msg, UInt16 msg_len) {    
    IPTSResponse *rsp = reinterpret_cast<IPTSResponse *>(msg);
    if (isResponseError(rsp))
        return;
    
    IOReturn ret = kIOReturnSuccess;
    switch (rsp->code) {
        case IPTS_RSP_GET_DEVICE_INFO: {
            IPTSGetDeviceInfoResponse device_info;
            memcpy(&device_info, rsp->payload, sizeof(device_info));
            touch_screen->vendor_id = device_info.vendor_id;
            touch_screen->device_id = device_info.device_id;
            touch_screen->version = device_info.intf_eds;
            touch_screen->max_contacts = device_info.max_contacts;
            
            if (allocateDMAResources(device_info.data_size, device_info.feedback_size) != kIOReturnSuccess) {
                LOG("Failed to allocate resources");
                ret = kIOReturnNoMemory;
                break;
            }
            
//            if (device_info.intf_eds > 1) {
//                IPTSGetReportDescriptorCommand get_desc;
//                memset(&get_desc, 0, sizeof(get_desc));
//                memset(report_desc_buffer.vaddr, 0, report_desc_buffer.len);
//
//                get_desc.addr_lower = report_desc_buffer.paddr & 0xffffffff;
//                get_desc.addr_upper = report_desc_buffer.paddr >> 32;
//                get_desc.padding_len = IPTS_REPORT_DESC_PADDING;
//                ret = sendIPTSCommand(IPTS_CMD_GET_REPORT_DESC, reinterpret_cast<UInt8 *>(&get_desc), sizeof(get_desc));
//                break;
//            }
            
            IPTSSetModeCommand set_mode;
            memset(&set_mode, 0, sizeof(set_mode));
            set_mode.mode = mode;
            ret = sendIPTSCommand(IPTS_CMD_SET_MODE, reinterpret_cast<UInt8 *>(&set_mode), sizeof(set_mode));
            break;
        }
//        case IPTS_RSP_GET_REPORT_DESC: {
//            IPTSDataHeader *header = reinterpret_cast<IPTSDataHeader *>(report_desc_buffer.vaddr);
//
//            if (header->type != IPTSDataTypeDescriptor) {
//                ret = kIOReturnInvalid;
//                break;
//            }
//            touch_screen->descriptor = &header->data[IPTS_REPORT_DESC_PADDING];
//            touch_screen->descriptor_size = header->size - IPTS_REPORT_DESC_PADDING;
//
//            IPTSSetModeCommand set_mode;
//            memset(&set_mode, 0, sizeof(set_mode));
//            set_mode.mode = mode;
//            ret = sendIPTSCommand(IPTS_CMD_SET_MODE, reinterpret_cast<UInt8 *>(&set_mode), sizeof(set_mode));
//            break;
//        }
        case IPTS_RSP_SET_MODE: {
            IPTSSetMemoryWindowCommand set_mem;
            memset(&set_mem, 0, sizeof(set_mem));
            for (int i = 0; i < IPTS_BUFFER_NUM; i++) {
                set_mem.data_buffer_addr_lower[i] = rx_buffer[i].paddr & 0xffffffff;
                set_mem.data_buffer_addr_upper[i] = rx_buffer[i].paddr >> 32;

                set_mem.feedback_buffer_addr_lower[i] = feedback_buffer[i].paddr & 0xffffffff;
                set_mem.feedback_buffer_addr_upper[i] = feedback_buffer[i].paddr >> 32;
            }
            set_mem.workqueue_addr_lower = workqueue_buffer.paddr & 0xffffffff;
            set_mem.workqueue_addr_upper = workqueue_buffer.paddr >> 32;

            set_mem.doorbell_addr_lower = doorbell_buffer.paddr & 0xffffffff;
            set_mem.doorbell_addr_upper = doorbell_buffer.paddr >> 32;

            set_mem.host2me_addr_lower = tx_buffer.paddr & 0xffffffff;
            set_mem.host2me_addr_upper = tx_buffer.paddr >> 32;

            set_mem.workqueue_size = IPTS_WORKQUEUE_SIZE;
            set_mem.workqueue_item_size = IPTS_WORKQUEUE_ITEM_SIZE;

            ret = sendIPTSCommand(IPTS_CMD_SET_MEM_WINDOW, reinterpret_cast<UInt8 *>(&set_mem), sizeof(set_mem));
            break;
        }
        case IPTS_RSP_SET_MEM_WINDOW:
            ret = sendIPTSCommand(IPTS_CMD_READY_FOR_DATA, nullptr, 0);
            if (ret != kIOReturnSuccess)
                break;
            
            if (!touch_screen_started) {
                if (!touch_screen->start(this)) {
                    touch_screen->detach(this);
                    LOG("Could not start Surface Touch Screen device");
                    ret = kIOReturnError;
                    break;
                }
                touch_screen_started = true;
            }
            
            LOG("IPTS Device is ready");
            state = IPTSDeviceStateStarted;
            
//            if (touch_screen->vendor_id == 0x045e && touch_screen->device_id & 0x0100)
//                ret = sendSetFeatureReport(0x5, 0x1);   // magic command to enable multitouch on newer devices
//            if (ret != kIOReturnSuccess)
//                LOG("Failed to enable multitouch on SP7 and newer devices!");
            
            if (mode == IPTSModeDoorbell) {
                timer->enable();
                timer->setTimeoutMS(IPTS_IDLE_TIMEOUT);
            }
            break;
        case IPTS_RSP_READY_FOR_DATA:
            if (mode == IPTSModeEvent) {
                LOG("Warning, event mode unsupported as it is too lossy and buggy!");
                // Event mode:
                // get current doorbell, manually increase it by 1
                // use doorbell % IPTS_BUFFER_NUM to get corresponding buffer and process it
                // send IPTS_CMD_READY_FOR_DATA for requesting new data
            }
            break;
        case IPTS_RSP_FEEDBACK: {
            if (state != IPTSDeviceStateStopping)
                break;
            
            IPTSFeedbackResponse feedback;
            memcpy(&feedback, rsp->payload, sizeof(feedback));
            if (feedback.buffer < IPTS_BUFFER_NUM - 1) {
                ret = refillBuffer(feedback.buffer + 1);
            } else if (feedback.buffer == IPTS_BUFFER_NUM - 1) {
                IPTSFeedbackHeader *header;
                memset(tx_buffer.vaddr, 0, tx_buffer.len);
                
                header = reinterpret_cast<IPTSFeedbackHeader *>(tx_buffer.vaddr);
                header->cmd_type = IPTSFeedbackCommandTypeSoftReset;
                header->buffer = IPTS_TX_BUFFER;
                ret = sendFeedback(IPTS_TX_BUFFER);
            } else
                ret = sendIPTSCommand(IPTS_CMD_CLEAR_MEM_WINDOW, nullptr, 0);
            break;
        }
        case IPTS_RSP_CLEAR_MEM_WINDOW:
            freeDMAResources();
            state = IPTSDeviceStateStopped;
            if (restart)
                ret = startDevice();
            break;
        default:
            LOG("Unhandled response code: 0x%08x", rsp->code);
            break;
    }
    if (ret != kIOReturnSuccess) {
        LOG("Error while handling response 0x%08x", rsp->code);
        stopDevice();
    }
}

bool IntelPreciseTouchStylusDriver::isResponseError(IPTSResponse *rsp) {
    bool error;
    switch (rsp->status) {
    case IPTSCommandSuccess:
    case IPTSCommandCompatCheckFail:
        error = false;
        break;
    case IPTSCommandInvalidParams:
        error = rsp->code != IPTS_RSP_FEEDBACK;
        break;
    case IPTSCommandSensorDisabled:
        error = state != IPTSDeviceStateStopping;
        break;
    default:
        error = true;
        break;
    }
    if (!error)
        return false;

    LOG("Command 0x%08x failed: %d", rsp->code, rsp->status);
    if (rsp->status == IPTSCommandExpectedReset || rsp->status == IPTSCommandUnexpectedReset) {
        LOG("Sensor was reset");
        restartDevice();
    }
    return true;
}

IOReturn IntelPreciseTouchStylusDriver::allocateDMAMemory(IPTSBufferInfo *info, UInt32 size) {
    IOBufferMemoryDescriptor *buf = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous | kIOMemoryKernelUserShared | kIOMapInhibitCache, size, DMA_BIT_MASK(64));
    if (!buf)
        return kIOReturnNoMemory;
    buf->prepare();
    
    IODMACommand *cmd = IODMACommand::withSpecification(kIODMACommandOutputHost64, 64, 0, IODMACommand::kMapped, 0, 4);
    if (!cmd) {
        buf->complete();
        OSSafeReleaseNULL(buf);
        return kIOReturnNoMemory;
    }
    cmd->setMemoryDescriptor(buf);

    IODMACommand::Segment64 seg;
    UInt64 offset = 0;
    UInt32 seg_num = 1;
    if (cmd->gen64IOVMSegments(&offset, &seg, &seg_num) != kIOReturnSuccess) {
        OSSafeReleaseNULL(cmd);
        buf->complete();
        OSSafeReleaseNULL(buf);
        return kIOReturnNoMemory;
    }
    info->paddr = seg.fIOVMAddr;
    info->vaddr = buf->getBytesNoCopy();
    info->len = size;
    info->buffer = buf;
    info->dma_cmd = cmd;

    return kIOReturnSuccess;
}

IOReturn IntelPreciseTouchStylusDriver::allocateDMAResources(UInt32 dbuff_size, UInt32 fbuff_size)
{
    for (int i = 0; i < IPTS_BUFFER_NUM; i++) {
        if (allocateDMAMemory(rx_buffer+i, dbuff_size) != kIOReturnSuccess)
            goto release_resources;
    }

    for (int i = 0; i < IPTS_BUFFER_NUM; i++) {
        if (allocateDMAMemory(feedback_buffer+i, fbuff_size) != kIOReturnSuccess)
            goto release_resources;
    }

    if (allocateDMAMemory(&doorbell_buffer, sizeof(UInt32)) != kIOReturnSuccess ||
        allocateDMAMemory(&workqueue_buffer, sizeof(UInt32)) != kIOReturnSuccess ||
        allocateDMAMemory(&tx_buffer, fbuff_size) != kIOReturnSuccess ||
        allocateDMAMemory(&report_desc_buffer, dbuff_size + 8) != kIOReturnSuccess)
        goto release_resources;
    
    input_buffer = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous | kIOMemoryKernelUserShared | kIOMapInhibitCache, dbuff_size, DMA_BIT_MASK(64));
    if (!input_buffer)
        goto release_resources;
    input_buffer->prepare();

    return kIOReturnSuccess;
release_resources:
    freeDMAResources();
    return kIOReturnNoMemory;
}

void IntelPreciseTouchStylusDriver::freeDMAMemory(IPTSBufferInfo *info, bool keep_ref) {
    info->dma_cmd->clearMemoryDescriptor();
    OSSafeReleaseNULL(info->dma_cmd);
    info->buffer->complete();
    if (keep_ref)
        info->buffer->release();    // maybe still hold by client, so needed when client wants to release them
    else
        OSSafeReleaseNULL(info->buffer);
    info->vaddr = nullptr;
    info->paddr = 0;
}

void IntelPreciseTouchStylusDriver::freeDMAResources()
{
    IPTSBufferInfo *buffers = rx_buffer;
    for (int i = 0; i < IPTS_BUFFER_NUM; i++) {
        if (!buffers[i].vaddr)
            continue;
        freeDMAMemory(buffers+i, true);
    }
    buffers = feedback_buffer;
    for (int i = 0; i < IPTS_BUFFER_NUM; i++) {
        if (!buffers[i].vaddr)
            continue;
        freeDMAMemory(buffers+i);
    }

    if (doorbell_buffer.vaddr)
        freeDMAMemory(&doorbell_buffer);
    if (workqueue_buffer.vaddr)
        freeDMAMemory(&workqueue_buffer);
    if (tx_buffer.vaddr)
        freeDMAMemory(&tx_buffer);
    if (report_desc_buffer.vaddr)
        freeDMAMemory(&report_desc_buffer);
    
    if (input_buffer) {
        input_buffer->complete();
        OSSafeReleaseNULL(input_buffer);
    }
}
