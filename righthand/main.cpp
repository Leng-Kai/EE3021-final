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

const static char DEVICE_NAME[] = "RightHand";

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
        printf("kaikai | start\n");
        _ble.init(this, &HeartrateDemo::on_init_complete);

        // _event_queue.dispatch_forever();
    }

    void hihat()
    {
        printf("hihat_open: %d\n", hihat_open);
        _heartrate_service.updateHeartRate(hihat_open);
    }

    void crash()
    {
        _heartrate_service.updateHeartRate1(1);
    }

    events::EventQueue &_event_queue;

private:
    /** Callback triggered when the ble initialization process has finished */
    void on_init_complete(BLE::InitializationCompleteCallbackContext *params)
    {
        printf("kaikai | on_init_complete\n");
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

volatile int32_t hihatXYZ[3];
volatile int32_t crashXYZ[3];
volatile int32_t hihat_norm;
volatile int32_t crash_norm;

volatile int phase = 2;
volatile bool hihat_done = false;
volatile bool crash_done = false;
volatile bool background_done = false;

int16_t pDataXYZ[3] = {0};
int16_t backgroundDataXYZ[3] = {0};
float pGyroDataXYZ[3] = {0};

volatile int total_sample0 = 0;
volatile int total_sample1 = 0;

int32_t hihatX[64], hihatY[64], hihatZ[64];
int32_t crashX[64], crashY[64], crashZ[64];

void button_released()
{
    if (phase == 0) {
        hihat_done = true;
    } else if (phase == 1) {
        crash_done = true;
    } else if (phase == 2) {
        background_done = true;
    }
    phase = (phase + 1) % 3;
}

void event_go(void const *arg) {
    demo->_event_queue.dispatch_forever();
}

void background_calculate(void const *arg) {
    while (1) {
        while (!background_done);

        BSP_ACCELERO_AccGetXYZ(backgroundDataXYZ);
        background_done = false;
    }
}

void hihat_calculate(void const *arg) {
    while (1) {
        while (!hihat_done);

        printf("hihat_cal total_sample0: %d\n", total_sample0);
        int total_sample = total_sample0;
        int sample_num = min(total_sample, 64);

        int sum = 0;
        for (int i = 0; i < sample_num; i++) {
            sum += hihatX[i];
        }
        hihatXYZ[0] = sum / sample_num;
        sum = 0;
        for (int i = 0; i < sample_num; i++) {
            sum += hihatY[i];
        }
        hihatXYZ[1] = sum / sample_num;
        sum = 0;
        for (int i = 0; i < sample_num; i++) {
            sum += hihatZ[i];
        }
        hihatXYZ[2] = sum / sample_num;
        hihat_norm = (hihatXYZ[0] * hihatXYZ[0] + hihatXYZ[1] * hihatXYZ[1] + hihatXYZ[2] * hihatXYZ[2]) / 30000;
            
        total_sample0 = 0;
        hihat_done = false;
        printf("\nhihat avg: X = %d, Y = %d, Z = %d, norm = %d\n", hihatXYZ[0], hihatXYZ[1], hihatXYZ[2], hihat_norm);
    }
}

void crash_calculate(void const *arg) {
    while (1) {
        while (!crash_done);

        printf("crash_cal total_sample1: %d\n", total_sample1);
        int total_sample = total_sample1;
        int sample_num = min(total_sample, 64);

        int sum = 0;
        for (int i = 0; i < sample_num; i++) {
            sum += crashX[i];
        }
        crashXYZ[0] = sum / sample_num;
        sum = 0;
        for (int i = 0; i < sample_num; i++) {
            sum += crashY[i];
        }
        crashXYZ[1] = sum / sample_num;
        sum = 0;
        for (int i = 0; i < sample_num; i++) {
            sum += crashZ[i];
        }
        crashXYZ[2] = sum / sample_num;
        crash_norm = (crashXYZ[0] * crashXYZ[0] + crashXYZ[1] * crashXYZ[1] + crashXYZ[2] * crashXYZ[2]) / 30000;
            
        total_sample1 = 0;
        crash_done = false;
        printf("\ncrash avg: X = %d, Y = %d, Z = %d, norm = %d\n", crashXYZ[0], crashXYZ[1], crashXYZ[2], crash_norm);
    }
}

void phase0_thread(void const *arg)
{
    while (1) {
        while (phase != 0);

        BSP_ACCELERO_AccGetXYZ(pDataXYZ);
        BSP_GYRO_GetXYZ(pGyroDataXYZ);

        int acc_sq = pDataXYZ[0] * pDataXYZ[0] + pDataXYZ[1] * pDataXYZ[1] + pDataXYZ[2] * pDataXYZ[2];
        if (acc_sq > 3000000) {
            hihatX[total_sample0 & 63] = (int16_t)pDataXYZ[0] - (int16_t)backgroundDataXYZ[0];
            hihatY[total_sample0 & 63] = (int16_t)pDataXYZ[1] - (int16_t)backgroundDataXYZ[1];
            hihatZ[total_sample0 & 63] = (int16_t)pDataXYZ[2] - (int16_t)backgroundDataXYZ[2];
            demo->hihat();
            printf("\nGYRO_X = %d\nGYRO_Y = %d\nGYRO_Z = %d\n", hihatX[total_sample0 & 63], hihatY[total_sample0 & 63], hihatZ[total_sample0 & 63]);
            total_sample0++;
            ThisThread::sleep_for(200);
        }
    }
}

void phase1_thread(void const *arg)
{
    while (1) {
        while (phase != 1);

        BSP_ACCELERO_AccGetXYZ(pDataXYZ);
        BSP_GYRO_GetXYZ(pGyroDataXYZ);

        int acc_sq = pDataXYZ[0] * pDataXYZ[0] + pDataXYZ[1] * pDataXYZ[1] + pDataXYZ[2] * pDataXYZ[2];
        if (acc_sq > 3000000) {
            crashX[total_sample1 & 63] = (int16_t)pDataXYZ[0] - (int16_t)backgroundDataXYZ[0];
            crashY[total_sample1 & 63] = (int16_t)pDataXYZ[1] - (int16_t)backgroundDataXYZ[1];
            crashZ[total_sample1 & 63] = (int16_t)pDataXYZ[2] - (int16_t)backgroundDataXYZ[2];
            demo->crash();
            printf("\nGYRO_X = %d\nGYRO_Y = %d\nGYRO_Z = %d\n", crashX[total_sample1 & 63], crashY[total_sample1 & 63], crashZ[total_sample1 & 63]);
            total_sample1++;
            ThisThread::sleep_for(200);
        }
    }
}

void phase2_thread(void const *arg)
{
    while (1) {
        while (phase != 2);

        BSP_ACCELERO_AccGetXYZ(pDataXYZ);
        BSP_GYRO_GetXYZ(pGyroDataXYZ);

        int acc_sq = pDataXYZ[0] * pDataXYZ[0] + pDataXYZ[1] * pDataXYZ[1] + pDataXYZ[2] * pDataXYZ[2];
        if (acc_sq > 3000000) {
            pDataXYZ[0] -= backgroundDataXYZ[0];
            pDataXYZ[1] -= backgroundDataXYZ[1];
            pDataXYZ[2] -= backgroundDataXYZ[2];
            // printf("\nGYRO_X = %d\nGYRO_Y = %d\nGYRO_Z = %d\n", pDataXYZ[0], pDataXYZ[1], pDataXYZ[2]);
            int32_t cos_hihat = (pDataXYZ[0] * hihatXYZ[0] + pDataXYZ[1] * hihatXYZ[1] + pDataXYZ[2] * hihatXYZ[2]) * crash_norm;
            int32_t cos_crash = (pDataXYZ[0] * crashXYZ[0] + pDataXYZ[1] * crashXYZ[1] + pDataXYZ[2] * crashXYZ[2]) * hihat_norm;
            // printf("cos_hihat: %lld,  cos_crash: %lld", cos_hihat, cos_crash);
            if (cos_hihat > cos_crash) {
                printf("\nhihat!\n");
                demo->hihat();
            } else {
                printf("\ncrash!\n");
                demo->crash();
            }
            ThisThread::sleep_for(200);
        }
    }
}

InterruptIn bu(D2);

void pressed() {
    hihat_open = false;
}

void released() {
    hihat_open = true;
}

int main()
{
    bu.rise(&pressed);
    bu.fall(&released);

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

    button.rise(&button_released);

    int a = 1;
    ev.start(callback(event_go, (void *)&a));
    t0.start(callback(phase0_thread, (void *)&a));
    t1.start(callback(phase1_thread, (void *)&a));
    t2.start(callback(phase2_thread, (void *)&a));
    c0.start(callback(hihat_calculate, (void *)&a));
    c1.start(callback(crash_calculate, (void *)&a));
    c2.start(callback(background_calculate, (void *)&a));

    while (1) {
        if (phase != 2) {
            led = !led;
        } else {
            led = 0;
        }
        ThisThread::sleep_for(500);
    }
}

