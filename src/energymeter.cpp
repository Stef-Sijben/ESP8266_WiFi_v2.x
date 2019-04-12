#include "energymeter.h"

#include "debug.h"
#include "input.h"
#include "mqtt.h"

EnergyMeter *energyMeters[MaxEnergyMeters];

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
    // Keep track of the value of elapsed between calls to decide when a new session starts
    static long lastElapsed = 0;
    const bool newSession = elapsed < lastElapsed;
    lastElapsed = elapsed;

    DBUGLN("Updating energy meters");
    

    unsigned long startTime = millis();
    for (size_t meterIndex = 0; meterIndex < MaxEnergyMeters; ++meterIndex) {
        if (energyMeters[meterIndex] != nullptr) {
            EnergyMeter &meter = *energyMeters[meterIndex];
            if (newSession) {
                mqtt_publish("energymeters/" + meter.name + "/newsession:" + String(meter.sessionEnergy()));
                meter.startSession();
            }

            if (meter.update()) {
                DBUGF("Energy meter %s updated", meter.name.c_str());

                // Collect a message with the updated values to publish via MQTT
                String mqttData;
                const String topic("energymeters/" + meter.name + '/');
                for (int dataPointIndex = 0; dataPointIndex < NDataPointTypes; ++dataPointIndex) {
                    const EnergyMeterDataPoint &dataPoint = meter.data(static_cast<DataPointType>(dataPointIndex));
                    if (dataPoint.lastUpdated >= startTime) {
                        mqttData = topic + DataPointTypeNames[dataPointIndex] + ':';
                        for (int i = 0; i < 4; ++i) {
                            mqttData += String(dataPoint.values[i]) + ' ';
                        }
                        // Strip off the final ' ' character
                        mqttData.remove(mqttData.length() - 1);
                        mqtt_publish(mqttData);
                    }
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

void EnergyMeter::startSession() {
    sessionStartEnergy = dataPoints[ENERGY].values[0];
}

float EnergyMeter::sessionEnergy() const {
    if (isnan(sessionStartEnergy)) {
        // We don't know the count at session start, so return 0
        return 0.0f;
    }
    return dataPoints[ENERGY].values[0] - sessionStartEnergy;
}
