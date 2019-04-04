#pragma once

#include "energymeter.h"

#include <SDM.h>

#include <Arduino.h>
#include <math.h>

enum SDMMeterType {
    SDM120,
    SDM220,
    SDM630,

    NSDMMeterTypes
};

class SDMMeter : public EnergyMeter {
    // The actual device instance from the SDM library
    SDM &dev;
    const SDMMeterType type;
    const uint8_t modbusAddr;
    
    // Each iteration of the loop updates one data point.
    // Track which is to be updated next.
    DataPointType nextUpdate = VOLTAGE;
public:
    SDMMeter(SDM &sdmDevice, SDMMeterType type, uint8_t addr, const String &name);

    bool updateField(DataPointType field);
    // Update one data point in the internal state
    bool update() override;
};
