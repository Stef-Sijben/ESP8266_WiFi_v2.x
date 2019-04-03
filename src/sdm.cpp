#include "sdm.h"

#include "debug.h"
#include "mqtt.h"

EnergyMeter *energyMeters[MaxEnergyMeters];

// Define mapping of generic fields to meter-specific registers
constexpr unsigned short EmptyDataPoint = 0xffff;
struct SDMDataPointMapping {
    const unsigned short registers[4];
};

constexpr SDMDataPointMapping deviceRegisterMap[NSDMMeterTypes][NDataPointTypes] = {
    { // SDM120
        { SDM120C_VOLTAGE,                 SDM120C_VOLTAGE,                 EmptyDataPoint, EmptyDataPoint },
        { SDM120C_CURRENT,                 SDM120C_CURRENT,                 EmptyDataPoint, EmptyDataPoint },
        { SDM120C_POWER,                   SDM120C_POWER,                   EmptyDataPoint, EmptyDataPoint },
        { SDM120C_ACTIVE_APPARENT_POWER,   SDM120C_ACTIVE_APPARENT_POWER,   EmptyDataPoint, EmptyDataPoint },
        { SDM120C_REACTIVE_APPARENT_POWER, SDM120C_REACTIVE_APPARENT_POWER, EmptyDataPoint, EmptyDataPoint },
        { SDM120C_POWER_FACTOR,            SDM120C_POWER_FACTOR,            EmptyDataPoint, EmptyDataPoint },
        // Several overall fields in one data point, no per-phase info
        { SDM120C_IMPORT_ACTIVE_ENERGY,    SDM120C_EXPORT_ACTIVE_ENERGY,    EmptyDataPoint, EmptyDataPoint },
    },
    { // SDM220
        { SDM220T_VOLTAGE,                 SDM220T_VOLTAGE,                 EmptyDataPoint, EmptyDataPoint },
        { SDM220T_CURRENT,                 SDM220T_CURRENT,                 EmptyDataPoint, EmptyDataPoint },
        { SDM220T_POWER,                   SDM220T_POWER,                   EmptyDataPoint, EmptyDataPoint },
        { SDM220T_ACTIVE_APPARENT_POWER,   SDM220T_ACTIVE_APPARENT_POWER,   EmptyDataPoint, EmptyDataPoint },
        { SDM220T_REACTIVE_APPARENT_POWER, SDM220T_REACTIVE_APPARENT_POWER, EmptyDataPoint, EmptyDataPoint },
        { SDM220T_POWER_FACTOR,            SDM220T_POWER_FACTOR,            EmptyDataPoint, EmptyDataPoint },
        // Several overall fields in one data point, no per-phase info
        { SDM220T_IMPORT_ACTIVE_ENERGY,    SDM220T_EXPORT_ACTIVE_ENERGY,    SDM220T_IMPORT_REACTIVE_ENERGY, SDM220T_EXPORT_REACTIVE_ENERGY }
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

float EnergyMeterDataPoint::sum() const {
    float sum = 0.0;
    for (int phase = 1; phase <= 3; ++ phase) {
        if (!isnan(values[phase])) {
            sum += values[phase];
        }
    }
    return sum;
}

float EnergyMeterDataPoint::avg() const {
    float sum = 0.0;
    unsigned char count = 0;
    for (int phase = 1; phase <= 3; ++ phase) {
        if (!isnan(values[phase])) {
            sum += values[phase];
            ++count;
        }
    }
    return sum / count;
}

float EnergyMeterDataPoint::min() const {
    float m = INFINITY;
    for (int phase = 1; phase <= 3; ++ phase) {
        if (!isnan(values[phase]) && values[phase] < m) {
            m = values[phase];
        }
    }
    return m;
}

float EnergyMeterDataPoint::max() const {
    float m = -INFINITY;
    for (int phase = 1; phase <= 3; ++ phase) {
        if (!isnan(values[phase]) && values[phase] > m) {
            m = values[phase];
        }
    }
    return m;
}

void registerEnergyMeter(EnergyMeter *meter) {
    for (size_t i = 0; i < MaxEnergyMeters; ++i) {
        if (energyMeters[i] == meter) {
            DBUGF("Energy meter %s already registered at index %d", meter->name.c_str(), i);
            return; // already registered
        } else if (energyMeters[i] == nullptr) {
            // Found an empty slot to register the new meter, save it and finish up
            DBUGF("Registering energy meter %s at index %d", meter->name.c_str(), i);
            energyMeters[i] = meter;
            return;
        }
    }
    DBUGF("Unable to register energy meter %s. No slots available", meter->name.c_str());
}

void updateEnergyMeters() {
    DBUGLN("Updating energy meters");

    unsigned long startTime = millis();
    for (size_t meterIndex = 0; meterIndex < MaxEnergyMeters; ++meterIndex) {
        if (energyMeters[meterIndex] != nullptr) {
            EnergyMeter &meter = *energyMeters[meterIndex];
            if (meter.update()) {
                DBUGF("Energy meter %s updated", meter.name.c_str());

                // Collect a message with the updated values to publish via MQTT
                String mqttData;
                const String topic("energymeters/" + meter.name + '/');
                for (int dataPointIndex = 0; dataPointIndex < NDataPointTypes; ++dataPointIndex) {
                    const EnergyMeterDataPoint &dataPoint = meter.data(static_cast<DataPointType>(dataPointIndex));
                    if (dataPoint.lastUpdated >= startTime) {
                        mqttData += topic + DataPointTypeNames[dataPointIndex] + ':';
                        for (int i = 0; i < 4; ++i) {
                            mqttData += String(dataPoint.values[i]) + ' ';
                        }
                        mqttData[mqttData.length()-1] = ',';
                    }
                }
                
                if (mqttData.length() > 0) {
                    // Strip off the final ',' character
                    mqttData.remove(mqttData.length() - 1);
                    mqtt_publish(mqttData);
                }
            }
        }
    }
}

const EnergyMeter *getEnergyMeter(size_t i) {
    if (i < MaxEnergyMeters) {
        return energyMeters[i];
    }
    return nullptr;
}

EnergyMeter::EnergyMeter(const String &name) : name(name) {
    for (size_t i = 0; i < NDataPointTypes; ++i) {
        dataPoints[i].type = static_cast<DataPointType>(i);
    }
}

const EnergyMeterDataPoint &EnergyMeter::data(DataPointType fieldName) const {
    return dataPoints[fieldName];
}

SDMMeter::SDMMeter(SDM &sdmDevice, SDMMeterType type, uint8_t addr, const String &name)
        : EnergyMeter(name), dev(sdmDevice), type(type), modbusAddr(addr) {
    DBUGF("Created SDMMeter %s at addres %d", name.c_str(), addr);
}

bool SDMMeter::update() {
    bool updated = false;

    size_t updateField = nextUpdate;
    DBUGF("SDMMeter %s@%d: Updating field %s", name.c_str(), modbusAddr, DataPointTypeNames[updateField]);
    const SDMDataPointMapping &updateFields = deviceRegisterMap[type][updateField];
    EnergyMeterDataPoint &dataPoint = dataPoints[updateField];

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

    // Next iteration we go update the next field, but wrap around at the end
    if (++updateField >= NDataPointTypes) {
        updateField = 0;
    }
    nextUpdate = static_cast<DataPointType>(updateField);

    return updated;
}