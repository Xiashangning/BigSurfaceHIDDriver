//
//  IntelPreciseTouchStylusDriver.hpp
//  SurfaceTouchScreen
//
//  Created by Xavier on 2022/6/5.
//  Copyright © 2022 Xia Shangning. All rights reserved.
//

#ifndef IntelPreciseTouchStylusDriver_hpp
#define IntelPreciseTouchStylusDriver_hpp

#include "../../../../BigSurface/BigSurface/SurfaceManagementEngine/SurfaceManagementEngineClient.hpp"
#include "IPTSProtocol.h"

enum IPTSDeviceState {
    IPTSDeviceStateStarting,
    IPTSDeviceStateStarted,
    IPTSDeviceStateStopping,
    IPTSDeviceStateStopped,
};

struct IPTSBufferInfo {
    IOBufferMemoryDescriptor* buffer;
    IODMACommand* dma_cmd;
    void*  vaddr;
    UInt16 len;
    IOPhysicalAddress paddr;
};

class SurfaceTouchScreenDevice;

class EXPORT IntelPreciseTouchStylusDriver : public IOService {
    OSDeclareDefaultStructors(IntelPreciseTouchStylusDriver);
    
public:
    IOService* probe(IOService* provider, SInt32* score) override;
    
    bool start(IOService* provider) override;
    
    void stop(IOService* provider) override;
    
    IOReturn setPowerState(unsigned long whichState, IOService *whatDevice) override;
    
    IOBufferMemoryDescriptor *getReceiveBuffer();
    
    IOReturn getDeviceInfo(IPTSDeviceInfo *info);
    
    IOReturn waitInput(UInt64 *size);
    
    void enterMultitouch();
    void exitMultitouch();
    
    void handleHIDReport(const IPTSHIDReport *report);
    
    void processingStarted();
    void processingEnded();
    
private:
    SurfaceManagementEngineClient*  api {nullptr};
    
    IOWorkLoop*                 work_loop {nullptr};
    IOCommandGate*              command_gate {nullptr};
    IOCommandGate::Action       wait_input {nullptr};
    IOCommandGate::Action       handle_report {nullptr};
    IOInterruptEventSource*     report_interrupt {nullptr};
    IOInterruptEventSource*     status_interrupt {nullptr};
    IOTimerEventSource*         timer {nullptr};

    SurfaceTouchScreenDevice*   touch_screen {nullptr};
    bool touch_screen_started   {false};
    
    IPTSDeviceState state {IPTSDeviceStateStopped};
    bool awake {true};
    bool busy {false};
    bool restart {false};
    
    UInt32 current_doorbell {0};
    AbsoluteTime last_activate;
    
    bool wait {false};
    bool get_feature {false};
    UInt8 *feature_report {nullptr};
    
    IPTSTouchMode mode {IPTSModeDoorbell};
    bool multitouch {false};
    
    UInt16 input_size {0};
    IOBufferMemoryDescriptor *input_buffer {nullptr};
    bool daemon_processing {false};
    bool daemon_handled {true};
    
    IOBufferMemoryDescriptor *report_to_send {nullptr};
    bool sent {true};
    
    IPTSBufferInfo rx_buffer[IPTS_BUFFER_NUM];
    IPTSBufferInfo feedback_buffer[IPTS_BUFFER_NUM];
    IPTSBufferInfo doorbell_buffer;
    IPTSBufferInfo workqueue_buffer;
    IPTSBufferInfo tx_buffer;
    IPTSBufferInfo report_desc_buffer;
    
    void releaseResources();
      
    void pollTouchData(IOTimerEventSource* sender);
    
    IOReturn startDevice();
    void stopDevice();
    void restartDevice();
    
    IOReturn sendIPTSCommand(UInt32 code, UInt8 *data, UInt16 data_len, bool blocking = true);
    IOReturn sendFeedback(UInt32 buffer, bool blocking = true);
    
    IOReturn sendGetFeatureRequest(UInt8 report_id, UInt8 *report, UInt16 size);
    IOReturn sendSetFeatureReport(UInt8 report_id, UInt8 value);
    
    IOReturn refillBuffer(UInt32 buffer, bool blocking = true);
    
    IOReturn allocateDMAMemory(IPTSBufferInfo *info, UInt32 size);
    IOReturn allocateDMAResources(UInt32 dbuff_size, UInt32 fbuff_size);
    void freeDMAMemory(IPTSBufferInfo *info, bool keep_ref = false);
    void freeDMAResources();
    
    void handleMessage(SurfaceManagementEngineClient *sender, UInt8 *msg, UInt16 msg_len);
    bool isResponseError(IPTSResponse *rsp);
    
    IOReturn waitInputGated(UInt64 *size);
    IOReturn handleHIDReportGated(IPTSHIDReport *report);
    IOReturn getFeatureRequestGated(UInt8* report_id, UInt16* size);
    void handleInterruptReport(IOInterruptEventSource *sender, int count);
    void handleInterruptStatus(IOInterruptEventSource *sender, int count);
};

#endif /* IntelPreciseTouchStylusDriver_hpp */
