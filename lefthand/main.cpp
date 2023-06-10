/* mbed Microcontroller Library
 * Copyright (c) 2006-2015 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <events/mbed_events.h>
#include "ble/BLE.h"
#include "ble/gap/Gap.h"
#include "ble/services/HeartRateService.h"
#include "pretty_printer.h"
#include "mbed-trace/mbed_trace.h"

#include "stm32l475e_iot01_tsensor.h"
#include "stm32l475e_iot01_hsensor.h"
#include "stm32l475e_iot01_psensor.h"
#include "stm32l475e_iot01_magneto.h"
#include "stm32l475e_iot01_gyro.h"
#include "stm32l475e_iot01_accelero.h"

using namespace std::literals::chrono_literals;

const static char DEVICE_NAME[] = "LeftHand";

static events::EventQueue event_queue(/* event count */ 16 * EVENTS_EVENT_SIZE);

volatile bool hihat_open = true;

class HeartrateDemo : ble::Gap::EventHandler {
public:
    HeartrateDemo(BLE &ble, events::EventQueue &event_queue) :
        _ble(ble),
        _event_queue(event_queue),
        _heartrate_uuid(GattService::UUID_HEART_RATE_SERVICE),
        _heartrate_value(100),
        _heartrate_value1(100),
        _heartrate_value2(100),
        _heartrate_value3(100),
        _heartrate_service(ble, _heartrate_value, _heartrate_value1, _heartrate_value2, _heartrate_value3, HeartRateService::LOCATION_FINGER),
        _adv_data_builder(_adv_buffer)
    {
    }

    void start()
    {
        _ble.init(this, &HeartrateDemo::on_init_complete);

        // _event_queue.dispatch_forever();
    }

    void snare()
    {
        _heartrate_service.updateHeartRate(1);
    }

    void bass()
    {
        _heartrate_service.updateHeartRate1(1);
    }

    events::EventQueue &_event_queue;

private:
    /** Callback triggered when the ble initialization process has finished */
    void on_init_complete(BLE::InitializationCompleteCallbackContext *params)
    {
        if (params->error != BLE_ERROR_NONE) {
            printf("Ble initialization failed.");
            return;
        }

        print_mac_address();

        /* this allows us to receive events like onConnectionComplete() */
        _ble.gap().setEventHandler(this);

        /* heart rate value updated every second */
        // _event_queue.call_every(
        //     1000ms,
        //     [this] {
        //         update_sensor_value();
        //     }
        // );

        start_advertising();
    }

    void start_advertising()
    {
        /* Create advertising parameters and payload */

        ble::AdvertisingParameters adv_parameters(
            ble::advertising_type_t::CONNECTABLE_UNDIRECTED,
            ble::adv_interval_t(ble::millisecond_t(100))
        );

        _adv_data_builder.setFlags();
        _adv_data_builder.setAppearance(ble::adv_data_appearance_t::GENERIC_HEART_RATE_SENSOR);
        _adv_data_builder.setLocalServiceList({&_heartrate_uuid, 1});
        _adv_data_builder.setName(DEVICE_NAME);

        /* Setup advertising */

        ble_error_t error = _ble.gap().setAdvertisingParameters(
            ble::LEGACY_ADVERTISING_HANDLE,
            adv_parameters
        );

        if (error) {
            printf("_ble.gap().setAdvertisingParameters() failed\r\n");
            return;
        }

        error = _ble.gap().setAdvertisingPayload(
            ble::LEGACY_ADVERTISING_HANDLE,
            _adv_data_builder.getAdvertisingData()
        );

        if (error) {
            printf("_ble.gap().setAdvertisingPayload() failed\r\n");
            return;
        }

        /* Start advertising */

        error = _ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);

        if (error) {
            printf("_ble.gap().startAdvertising() failed\r\n");
            return;
        }

        printf("Heart rate sensor advertising, please connect\r\n");
    }

    void update_sensor_value()
    {
        /* you can read in the real value but here we just simulate a value */
        _heartrate_value++;

        /*  60 <= bpm value < 110 */
        if (_heartrate_value == 110) {
            _heartrate_value = 60;
        }

        int16_t pDataXYZ[3] = {0};
        BSP_MAGNETO_GetXYZ(pDataXYZ);

        printf("\nheartrate: %d\n", _heartrate_value);
        printf("MAGNETO_X = %d\n", pDataXYZ[0]);
        printf("MAGNETO_Y = %d\n", pDataXYZ[1]);
        printf("MAGNETO_Z = %d\n", pDataXYZ[2]);

        _heartrate_service.updateHeartRate(_heartrate_value);
        _heartrate_service.updateHeartRate1(pDataXYZ[0]);
        _heartrate_service.updateHeartRate2(pDataXYZ[1]);
        _heartrate_service.updateHeartRate3(pDataXYZ[2]);
    }

    /* these implement ble::Gap::EventHandler */
private:
    /* when we connect we stop advertising, restart advertising so others can connect */
    virtual void onConnectionComplete(const ble::ConnectionCompleteEvent &event)
    {
        if (event.getStatus() == ble_error_t::BLE_ERROR_NONE) {
            printf("Client connected, you may now subscribe to updates\r\n");
        }
    }

    /* when we connect we stop advertising, restart advertising so others can connect */
    virtual void onDisconnectionComplete(const ble::DisconnectionCompleteEvent &event)
    {
        printf("Client disconnected, restarting advertising\r\n");

        ble_error_t error = _ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);

        if (error) {
            printf("_ble.gap().startAdvertising() failed\r\n");
            return;
        }
    }

private:
    BLE &_ble;

    UUID _heartrate_uuid;
    uint16_t _heartrate_value, _heartrate_value1, _heartrate_value2, _heartrate_value3;
    HeartRateService _heartrate_service;

    uint8_t _adv_buffer[ble::LEGACY_ADVERTISING_MAX_SIZE];
    ble::AdvertisingDataBuilder _adv_data_builder;
};

HeartrateDemo *demo;

/* Schedule processing of events from the BLE middleware in the event queue. */
void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *context)
{
    event_queue.call(Callback<void()>(&context->ble, &BLE::processEvents));
}

// ====== sensor ======

DigitalOut led(LED1);

InterruptIn button(BUTTON1);

Thread t0, t1, t2, c0, c1, c2, ev;

volatile bool bass_pressed = false;

int16_t pDataXYZ[3] = {0};

void event_go(void const *arg) {
    demo->_event_queue.dispatch_forever();
}

void bass_check(void const *arg) {
    while (1) {
        while (!bass_pressed);

        demo->bass();
        ThisThread::sleep_for(200);
        bass_pressed = false;
    }
}

InterruptIn bu(D2);

void pressed() {
    bass_pressed = true;
}

int main()
{
    bu.rise(&pressed);

    printf("Start sensor init\n");

    BSP_TSENSOR_Init();
    BSP_HSENSOR_Init();
    BSP_PSENSOR_Init();

    BSP_MAGNETO_Init();
    BSP_GYRO_Init();
    BSP_ACCELERO_Init();

    BLE &ble = BLE::Instance();
    ble.onEventsToProcess(schedule_ble_events);

    demo = new HeartrateDemo(ble, event_queue);
    demo->start();

    int a = 1;
    ev.start(callback(event_go, (void *)&a));
    c2.start(callback(bass_check, (void *)&a));

    while (1) {
        BSP_ACCELERO_AccGetXYZ(pDataXYZ);

        int acc_sq = pDataXYZ[0] * pDataXYZ[0] + pDataXYZ[1] * pDataXYZ[1] + pDataXYZ[2] * pDataXYZ[2];
        if (acc_sq > 3000000) {
            demo->snare();
            ThisThread::sleep_for(200);
        }
    }
}

