//
//  VoodooI2CTouchscreenHIDEventDriver.cpp
//  VoodooI2CHID
//
//  Created by blankmac on 9/30/17.
//  Copyright © 2017 Alexandre Daoud. All rights reserved.
//

#include "SurfaceTouchscreenHIDEventDriver.hpp"

#define super VoodooI2CMultitouchHIDEventDriver
OSDefineMetaClassAndStructors(SurfaceTouchscreenHIDEventDriver, VoodooI2CMultitouchHIDEventDriver);

UInt16 abs(SInt16 x) {
    if (x < 0)
        return x * -1;
    return x;
}

// Override of VoodooI2CMultitouchHIDEventDriver
bool SurfaceTouchscreenHIDEventDriver::checkFingerTouch(AbsoluteTime timestamp, VoodooI2CMultitouchEvent event) {
    bool got_transducer = false;
    
    for (int index = 0; index < event.transducers->getCount(); index++) {
        VoodooI2CDigitiserTransducer* transducer = OSDynamicCast(VoodooI2CDigitiserTransducer, event.transducers->getObject(index));
        if (!transducer)
            return false;

        if (transducer->type == kDigitiserTransducerFinger) {
            UInt16 value = transducer->tip_switch.value();
            UInt16 last_value = transducer->tip_switch.last.value;
            if (value || last_value) {
                got_transducer = true;
                
                // Convert logical coordinates to IOFixed and Scaled;
                IOFixed x = ((transducer->coordinates.x.value() * 1.0f) / transducer->logical_max_x) * 65535;
                IOFixed y = ((transducer->coordinates.y.value() * 1.0f) / transducer->logical_max_y) * 65535;
                
                checkRotation(&x, &y);
                
                // Double click routine
                if (!last_value) {  // touch starts
                    clock_get_uptime(&click_start);
                    touch_start_x = x;
                    touch_start_y = y;
                    
                    click_mask = false;
                    should_consider_long_press = true;
                    long_press_timeout = 0;
                }
                if (!value) {       // touch ends
                    AbsoluteTime click_end;
                    UInt64 nsecs;
                    clock_get_uptime(&click_end);
                    SUB_ABSOLUTETIME(&click_end, &click_start);
                    absolutetime_to_nanoseconds(click_end, &nsecs);
                    if (nsecs < 100000000) {    // < 100ms a quick click
                        clock_get_uptime(&click_end);
                        SUB_ABSOLUTETIME(&click_end, &last_quick_click);
                        absolutetime_to_nanoseconds(click_end, &nsecs);
                        if (nsecs < 200000000) {    // < 200ms between two quick clicks
                            
                            dispatchDigitizerEventWithTiltOrientation(timestamp, transducer->secondary_id, transducer->type, 0x1, 0x00, x, y);
                            
                            // double click event
                            dispatchDigitizerEventWithTiltOrientation(timestamp, transducer->secondary_id, transducer->type, 0x1, 0x01, x, y);
                            IOSleep(10);
                            dispatchDigitizerEventWithTiltOrientation(timestamp, transducer->secondary_id, transducer->type, 0x1, 0x00, x, y);
                            
                            IOSleep(50);
                            
                            dispatchDigitizerEventWithTiltOrientation(timestamp, transducer->secondary_id, transducer->type, 0x1, 0x01, x, y);
                            IOSleep(10);
                            // The second lift event will be executed below
                            
                            last_quick_click = 0;
                        } else
                            clock_get_uptime(&last_quick_click);
                    }
                }
                
                // Begin long press -> right click routine.
                // Increasing long_press_counter check will lengthen the time until execution.
                bool right_click = false;
                if (should_consider_long_press
                    && (abs(touch_start_x-x) > 2*RIGHT_CLICK_PRESS_RANGE
                        || abs(touch_start_y-y) > 2*RIGHT_CLICK_PRESS_RANGE))
                    should_consider_long_press = false;
                if (should_consider_long_press && !click_mask && value) {
                    if (long_press_timeout
                        && abs(long_press_x-x) < RIGHT_CLICK_PRESS_RANGE
                        && abs(long_press_y-y) < RIGHT_CLICK_PRESS_RANGE) {
                        AbsoluteTime cur_time;
                        clock_get_uptime(&cur_time);
                        if (cur_time > long_press_timeout) {
                            click_mask = true;
                            right_click = true;
                            
                            long_press_x = 0;
                            long_press_y = 0;
                        }
                    } else {
                        AbsoluteTime timeout;
                        nanoseconds_to_absolutetime(1000000000, &timeout);  // 1s
                        clock_get_uptime(&long_press_timeout);
                        ADD_ABSOLUTETIME(&long_press_timeout, &timeout);
                        
                        long_press_x = x;
                        long_press_y = y;
                    }
                }
                //  End long press -> right click routine.
                
                /* We need the first couple of single touch events to be in hover mode.
                 * In modes such as Mission Control, this allows us to select and drag
                 * windows vs just select and exit.
                 * We are mimicking a cursor being moved into position prior to executing a drag movement.
                 * There is little noticeable affect in other circumstances. */
                UInt32 buttons;
                if (right_click)
                    buttons = 0x02;
                else if (!last_value || click_mask)
                    buttons = 0x00;
                else
                    buttons = value;
                
                dispatchDigitizerEventWithTiltOrientation(timestamp, transducer->secondary_id, transducer->type, 0x1, buttons, x, y);
            }
        }
    }
    return got_transducer;
}

void SurfaceTouchscreenHIDEventDriver::checkRotation(IOFixed* x, IOFixed* y) {
    if (active_framebuffer) {
        if (current_rotation & kIOFBSwapAxes) {
            IOFixed old_x = *x;
            *x = *y;
            *y = old_x;
        }
        if (current_rotation & kIOFBInvertX)
            *x = 65535 - *x;
        if (current_rotation & kIOFBInvertY)
            *y = 65535 - *y;
    }
}

bool SurfaceTouchscreenHIDEventDriver::checkStylus(AbsoluteTime timestamp, VoodooI2CMultitouchEvent event) {
    //  Check the current transducers for stylus operation, dispatch the pointer events and return true.
    //  At this time, Apple has removed all methods of handling additional information from the event driver.  Only x, y, buttonstate, and
    //  inrange are valid for macOS Sierra +.  10.11 still makes use of extended functions.
    for (int index = 0, count = event.transducers->getCount(); index < count; index++) {
        VoodooI2CDigitiserTransducer* transducer = OSDynamicCast(VoodooI2CDigitiserTransducer, event.transducers->getObject(index));

        if (transducer->type == kDigitiserTransducerStylus && transducer->in_range) {
            VoodooI2CDigitiserStylus* stylus = (VoodooI2CDigitiserStylus*)transducer;
            IOFixed x = ((stylus->coordinates.x.value() * 1.0f) / stylus->logical_max_x) * 65535;
            IOFixed y = ((stylus->coordinates.y.value() * 1.0f) / stylus->logical_max_y) * 65535;
            IOFixed z = ((stylus->coordinates.z.value() * 1.0f) / stylus->logical_max_z) * 65535;
            IOFixed stylus_pressure = ((stylus->tip_pressure.value() * 1.0f) /stylus->pressure_physical_max) * 65535;
            
            checkRotation(&x, &y);
            
            if (stylus->barrel_switch.value() != 0x0 && stylus->barrel_switch.value() != 0x2 && (stylus->barrel_switch.value()-barrel_switch_offset) != 0x2)
                barrel_switch_offset = stylus->barrel_switch.value();
            if (stylus->eraser.value() != 0x0 && stylus->eraser.value() !=0x2 && (stylus->eraser.value()-eraser_switch_offset) != 0x4)
                eraser_switch_offset = stylus->eraser.value();
            
            stylus_buttons = stylus->tip_switch.value();
            
            if (stylus->barrel_switch.value() == 0x2 || (stylus->barrel_switch.value() - barrel_switch_offset) == 0x2) {
                stylus_buttons = 0x2;
            }
            
            if (stylus->eraser.value() == 0x4 || (stylus->eraser.value() - eraser_switch_offset) == 0x4) {
                stylus_buttons = 0x4;
            }
            
            dispatchDigitizerEventWithTiltOrientation(timestamp, stylus->secondary_id, stylus->type, stylus->in_range, stylus_buttons, x, y, z, stylus_pressure, stylus->barrel_pressure.value(), stylus->azi_alti_orientation.twist.value(), stylus->tilt_orientation.x_tilt.value(), stylus->tilt_orientation.y_tilt.value());
            
            return true;
        }
    }
    
    return false;
}

IOFramebuffer* SurfaceTouchscreenHIDEventDriver::getFramebuffer() {
    IORegistryEntry* display = NULL;
    IOFramebuffer* framebuffer = NULL;
    
    OSDictionary *match = serviceMatching("IODisplay");
    OSIterator *iterator = getMatchingServices(match);

    if (iterator) {
        display = OSDynamicCast(IORegistryEntry, iterator->getNextObject());
        
        if (display) {
            IOLog("%s::Got active display\n", getName());
            IORegistryEntry *entry = display->getParentEntry(gIOServicePlane)->getParentEntry(gIOServicePlane);
            if (entry)
                framebuffer = reinterpret_cast<IOFramebuffer*>(entry->metaCast("IOFramebuffer"));
            if (framebuffer)
                IOLog("%s::Got active framebuffer\n", getName());
        }
        
        iterator->release();
    }
    
    OSSafeReleaseNULL(match);
    return framebuffer;
}

void SurfaceTouchscreenHIDEventDriver::forwardReport(VoodooI2CMultitouchEvent event, AbsoluteTime timestamp) {
    if (!active_framebuffer)
        active_framebuffer = getFramebuffer();

    if (active_framebuffer) {
        OSNumber* number = OSDynamicCast(OSNumber, active_framebuffer->getProperty(kIOFBTransformKey));
        current_rotation = number->unsigned8BitValue() / 0x10;
        if (multitouch_interface)
            multitouch_interface->setProperty(kIOFBTransformKey, current_rotation, 8);
    }
    
    if (event.contact_count > 0) {
        // Send multitouch information to the multitouch interface
        if (event.contact_count == 1 && (checkStylus(timestamp, event) || checkFingerTouch(timestamp, event)))
            return;
        if (multitouch_interface)
            multitouch_interface->handleInterruptReport(event, timestamp);
    }
}

bool SurfaceTouchscreenHIDEventDriver::handleStart(IOService* provider) {
    if (!super::handleStart(provider))
        return false;
    
    active_framebuffer = getFramebuffer();
    
    return true;
}

void SurfaceTouchscreenHIDEventDriver::handleStop(IOService* provider) {
    super::handleStop(provider);
}
