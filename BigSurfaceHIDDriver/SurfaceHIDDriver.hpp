//
//  SurfaceHIDDriver.hpp
//  SurfaceHID
//
//  Created by Xavier on 2022/5/16.
//  Copyright © 2022 Xia Shangning. All rights reserved.
//

#ifndef SurfaceHIDDriver_hpp
#define SurfaceHIDDriver_hpp

#include "../../../BigSurface/BigSurface/SurfaceSerialHubDevices/SurfaceHIDNub.hpp"

class SurfaceHIDDevice;

class EXPORT SurfaceHIDDriver : public IOService {
	OSDeclareDefaultStructors(SurfaceHIDDriver)

public:
	IOService *probe(IOService *provider, SInt32 *score) override;
	
	bool start(IOService *provider) override;
	
	void stop(IOService *provider) override;
    
    IOReturn getHIDDescriptor(SurfaceHIDDeviceType device, SurfaceHIDDescriptor *desc);
    
    IOReturn getHIDAttributes(SurfaceHIDDeviceType device, SurfaceHIDAttributes *attr);
    
    IOReturn getReportDescriptor(SurfaceHIDDeviceType device, UInt8 *buffer, UInt16 len);
    
    IOReturn getRawReport(SurfaceHIDDeviceType device, UInt8 report_id, UInt8 *buffer, UInt16 len);
    
    void setRawReport(SurfaceHIDDeviceType device, UInt8 report_id, bool feature, UInt8 *buffer, UInt16 len);
    
private:
    SurfaceHIDNub*          nub {nullptr};
    IOWorkLoop*             work_loop {nullptr};
    IOInterruptEventSource* kbd_interrupt {nullptr};
    IOInterruptEventSource* tpd_interrupt {nullptr};
    SurfaceHIDDevice*       keyboard {nullptr};
    SurfaceHIDDevice*       touchpad {nullptr};
    
    bool    legacy {false};
    bool    ready {false};
    
    IOBufferMemoryDescriptor* kbd_report {nullptr};
    IOBufferMemoryDescriptor* tpd_report {nullptr};
    
    void eventReceived(SurfaceHIDNub *sender, SurfaceHIDDeviceType device, UInt8 *buffer, UInt16 len);
    
    void keyboardInputReceived(IOInterruptEventSource *sender, int count);
    
    void touchpadInputReceived(IOInterruptEventSource *sender, int count);
    
    void releaseResources();
};

#endif /* SurfaceHIDDriver_hpp */
