#include "sdm.h"

#include "debug.h"

// Define mapping of generic fields to meter-specific registers
constexpr unsigned short EmptyDataPoint = 0xffff;
struct SDMDataPointMapping {
    const unsigned short registers[4];
};

constexpr SDMDataPointMapping deviceRegisterMap[NSDMMeterTypes][NDataPointTypes] = {
    { // SDM120C
        { SDM120C_VOLTAGE,                 SDM120C_VOLTAGE,                 EmptyDataPoint, EmptyDataPoint },
        { SDM120C_CURRENT,                 SDM120C_CURRENT,                 EmptyDataPoint, EmptyDataPoint },
        { SDM120C_POWER,                   SDM120C_POWER,                   EmptyDataPoint, EmptyDataPoint },
        { SDM120C_ACTIVE_APPARENT_POWER,   SDM120C_ACTIVE_APPARENT_POWER,   EmptyDataPoint, EmptyDataPoint },
        { SDM120C_REACTIVE_APPARENT_POWER, SDM120C_REACTIVE_APPARENT_POWER, EmptyDataPoint, EmptyDataPoint },
        { SDM120C_POWER_FACTOR,            SDM120C_POWER_FACTOR,            EmptyDataPoint, EmptyDataPoint },
        // Several overall fields in one data point, no per-phase info
        { SDM120C_IMPORT_ACTIVE_ENERGY,    SDM120C_EXPORT_ACTIVE_ENERGY,    EmptyDataPoint, EmptyDataPoint },
    },
    { // SDM220T
        { SDM220T_VOLTAGE,                 SDM220T_VOLTAGE,                 EmptyDataPoint, EmptyDataPoint },
        { SDM220T_CURRENT,                 SDM220T_CURRENT,                 EmptyDataPoint, EmptyDataPoint },
        { SDM220T_POWER,                   SDM220T_POWER,                   EmptyDataPoint, EmptyDataPoint },
        { SDM220T_ACTIVE_APPARENT_POWER,   SDM220T_ACTIVE_APPARENT_POWER,   EmptyDataPoint, EmptyDataPoint },
        { SDM220T_REACTIVE_APPARENT_POWER, SDM220T_REACTIVE_APPARENT_POWER, EmptyDataPoint, EmptyDataPoint },
        { SDM220T_POWER_FACTOR,            SDM220T_POWER_FACTOR,            EmptyDataPoint, EmptyDataPoint },
        // Several overall fields in one data point, no per-phase info
        { SDM220T_IMPORT_ACTIVE_ENERGY,    SDM220T_EXPORT_ACTIVE_ENERGY,    SDM220T_IMPORT_REACTIVE_ENERGY, SDM220T_EXPORT_REACTIVE_ENERGY }
    },
    { // SDM230
        { SDM230_VOLTAGE,                 SDM230_VOLTAGE,                 EmptyDataPoint, EmptyDataPoint },
        { SDM230_CURRENT,                 SDM230_CURRENT,                 EmptyDataPoint, EmptyDataPoint },
        { SDM230_POWER,                   SDM230_POWER,                   EmptyDataPoint, EmptyDataPoint },
        { SDM230_ACTIVE_APPARENT_POWER,   SDM230_ACTIVE_APPARENT_POWER,   EmptyDataPoint, EmptyDataPoint },
        { SDM230_REACTIVE_APPARENT_POWER, SDM230_REACTIVE_APPARENT_POWER, EmptyDataPoint, EmptyDataPoint },
        { SDM230_POWER_FACTOR,            SDM230_POWER_FACTOR,            EmptyDataPoint, EmptyDataPoint },
        // Several overall fields in one data point, no per-phase info
        { SDM230_IMPORT_ACTIVE_ENERGY,    SDM230_EXPORT_ACTIVE_ENERGY,    SDM230_IMPORT_REACTIVE_ENERGY, SDM230_EXPORT_REACTIVE_ENERGY }
    },
    { // SDM630
        { SDM630_VOLTAGE_AVERAGE,          SDM630_VOLTAGE1,             SDM630_VOLTAGE2,            SDM630_VOLTAGE3 },
        { SDM630_CURRENTSUM,               SDM630_CURRENT1,             SDM630_CURRENT2,            SDM630_CURRENT3 },
        { SDM630_POWERTOTAL,               SDM630_POWER1,               SDM630_POWER2,              SDM630_POWER3   },
        { SDM630_VOLT_AMPS_TOTAL,          SDM630_VOLT_AMPS1,           SDM630_VOLT_AMPS2,          SDM630_VOLT_AMPS3 },
        { SDM630_VOLT_AMPS_REACTIVE_TOTAL, SDM630_VOLT_AMPS_REACTIVE1,  SDM630_VOLT_AMPS_REACTIVE2, SDM630_VOLT_AMPS_REACTIVE3 },
        { SDM630_POWER_FACTOR_TOTAL,       SDM630_POWER_FACTOR1,        SDM630_POWER_FACTOR2,       SDM630_POWER_FACTOR3 },
        // Several overall fields in one data point, no per-phase info
        { SDM630_IMPORT_ACTIVE_ENERGY,     SDM630_EXPORT_ACTIVE_ENERGY, SDM630_IMPORT_REACTIVE_ENERGY, SDM630_EXPORT_REACTIVE_ENERGY }
    }
};

SDMMeter::SDMMeter(SDM &sdmDevice, SDMMeterType type, uint8_t addr, const String &name)
        : EnergyMeter(name), dev(sdmDevice), type(type), modbusAddr(addr) {
    DBUGF("Created SDMMeter %s at addres %d", name.c_str(), addr);
}

bool SDMMeter::updateField(DataPointType field) {
    bool updated = false;

    DBUGF("SDMMeter %s@%d: Updating field %s", name.c_str(), modbusAddr, DataPointTypeNames[updateField]);
    const SDMDataPointMapping &updateFields = deviceRegisterMap[type][field];
    EnergyMeterDataPoint &dataPoint = dataPoints[field];

    if (millis() - dataPoint.lastUpdated >= 30000) {
        for (int i = 0; i < 4; ++i) {
            unsigned short registerAddr = updateFields.registers[i];

            if (registerAddr != EmptyDataPoint) {
                dataPoint.values[i] = dev.readVal(registerAddr, modbusAddr);
                // DBUGF("Read value %f from register %d", dataPoint.values[i], registerAddr);
                updated = true;
            }
            // TODO: Avoid duplicate requests for single-phase meters
        }
        if (updated) {
            dataPoint.lastUpdated = millis();
        }
    }

    return updated;
}

bool SDMMeter::update() {
    bool updated = updateField(nextUpdate);

    // Next iteration we go update the next field, but wrap around after the last
    nextUpdate = static_cast<DataPointType>((nextUpdate + 1) % NDataPointTypes);

    return updated;
}