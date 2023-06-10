import simplepyble
import time
import os
import socket
from pydub import AudioSegment
from pydub.playback import play
from appscript import app, k
import rtmidi

def bass():
    midiout.send_message([0x90, 36, 112])
    midiout.send_message([0x90, 36, 0])

def snare():
    midiout.send_message([0x90, 82, 112])
    midiout.send_message([0x90, 82, 0])

def hh_close():
    midiout.send_message([0x90, 90, 112])
    midiout.send_message([0x90, 90, 0])

def hh_open():
    midiout.send_message([0x90, 46, 112])
    midiout.send_message([0x90, 46, 0])

def hh(open):
    if int.from_bytes(open, byteorder='little'):
        hh_open()
    else:
        hh_close()

def crash():
    midiout.send_message([0x90, 97, 112])
    midiout.send_message([0x90, 97, 0])

if __name__ == "__main__":
    midiout = rtmidi.MidiOut()
    available_ports = midiout.get_ports()

    if available_ports:
        midiout.open_port(1)
    else:
        midiout.open_virtual_port("My virtual output")

    adapters = simplepyble.Adapter.get_adapters()

    if len(adapters) == 0:
        print("No adapters found")

    # Query the user to pick an adapter
    print("Please select an adapter:")
    for i, adapter in enumerate(adapters):
        print(f"{i}: {adapter.identifier()} [{adapter.address()}]")

    choice = int(input("Enter choice: "))
    adapter = adapters[choice]

    print(f"Selected adapter: {adapter.identifier()} [{adapter.address()}]")

    adapter.set_callback_on_scan_start(lambda: print("Scan started."))
    adapter.set_callback_on_scan_stop(lambda: print("Scan complete."))
    adapter.set_callback_on_scan_found(lambda peripheral: print(f"Found {peripheral.identifier()} [{peripheral.address()}]"))





    #
    # Select the left hand stm32
    #

    # Scan for 15 seconds
    adapter.scan_for(10000)
    peripherals = adapter.scan_get_results()

    # Query the user to pick a peripheral
    print("Please select a peripheral:")
    for i, peripheral in enumerate(peripherals):
        print(f"{i}: {peripheral.identifier()} [{peripheral.address()}]")

    choice = int(input("Enter choice: "))
    peripheral = peripherals[choice]

    print(f"Connecting to: {peripheral.identifier()} [{peripheral.address()}]")
    peripheral.connect()

    #
    # Select the snare characteristic
    #

    print("Successfully connected, listing services...")
    services = peripheral.services()
    service_characteristic_pair = []
    for service in services:
        for characteristic in service.characteristics():
            service_characteristic_pair.append((service.uuid(), characteristic.uuid()))

    # Query the user to pick a service/characteristic pair
    print("Please select a service/characteristic pair:")
    for i, (service_uuid, characteristic) in enumerate(service_characteristic_pair):
        print(f"{i}: {service_uuid} {characteristic}")

    choice = int(input("Enter choice: "))
    service_uuid, characteristic_uuid = service_characteristic_pair[choice]

    # Write the content to the characteristic
    contents = peripheral.notify(service_uuid, characteristic_uuid, lambda data: snare())

    #
    # Select the bass characteristic
    #

    print("Successfully connected, listing services...")
    services = peripheral.services()
    service_characteristic_pair = []
    for service in services:
        for characteristic in service.characteristics():
            service_characteristic_pair.append((service.uuid(), characteristic.uuid()))

    # Query the user to pick a service/characteristic pair
    print("Please select a service/characteristic pair:")
    for i, (service_uuid, characteristic) in enumerate(service_characteristic_pair):
        print(f"{i}: {service_uuid} {characteristic}")

    choice = int(input("Enter choice: "))
    service_uuid, characteristic_uuid = service_characteristic_pair[choice]

    # Write the content to the characteristic
    contents = peripheral.notify(service_uuid, characteristic_uuid, lambda data: bass())







    while True:
        continue

    peripheral.disconnect()
