#pragma once

#include <math.h>

#include <SDM.h>
#include <SoftwareSerial.h>

constexpr size_t MaxEnergyMeters = 10;

enum DataPointType {
    VOLTAGE,
    CURRENT,
    POWER_REAL,
    POWER_APPARENT,
    POWER_REACTIVE,
    POWER_FACTOR,

    NDataPointTypes
};

// All the measurements of an energy meter for one type of measurement
// Values that are not present (e.g. L2,L3 in a single-phase system) are set to NaN.
struct EnergyMeterDataPoint {
    // Last time this was updated as per millis()
    unsigned long lastUpdated = 0;

    // Total and per-phase values of this data point at the last update.
    // The meaning of the total depends on the exact data point,
    // e.g. average voltage, summed current, power etc.
    // NaN indicates no value available.
    float values[4] = { NAN, NAN, NAN, NAN }; // Total, L1, L2, L3

    // Collect some statistics over all phase's measurements
    float sum() const;
    float avg() const;
    float min() const;
    float max() const;
};

class EnergyMeter {
protected:
    // All the readings from this meter
    EnergyMeterDataPoint dataPoints[NDataPointTypes];

public:
    const EnergyMeterDataPoint &data(DataPointType fieldName) const;

    // Call this repeatedly to update the measurements.
    virtual void update() = 0;
};

void registerEnergyMeter(EnergyMeter *meter);
// Integrate this in the loop function to keep updating all meters' readings
void updateEnergyMeters();

enum SDMMeterType {
    SDM120,
    SDM220,
    SDM630,

    NSDMMeterTypes
};

class SDMMeter : public EnergyMeter {
    const SDMMeterType type;
    // The actual device instance from the SDM library
    SDM dev;
    
    // Each iteration of the loop updates one data point.
    // Track which is to be updated next.
    DataPointType nextUpdate = VOLTAGE;
public:
    SDMMeter(SDMMeterType type, SoftwareSerial &serial, long baudRate, int derePin = NOT_A_PIN);

    // Update one data point in the internal state
    void update() override;
};
