#
# This file is part of the MicroPython for Monocle project:
#      https://github.com/brilliantlabsAR/monocle-micropython
#
# Authored by: Josuah Demangeon (me@josuah.net)
#              Raj Nakarja / Brilliant Labs Ltd. (raj@itsbrilliant.co)
#
# ISC Licence
#
# Copyright © 2023 Brilliant Labs Ltd.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
# OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.
#

import device
import display
import camera
import microphone
import touch
import led
import fpga
import bluetooth
import time
import update
import math

def __test(evaluate, expected):
    try:
        response = eval(evaluate)
    except Exception as ex:
        response = type(ex)

    if response == expected:
        print(f"Passed - {evaluate} == {response}")
    else:
        print(f"Failed - {evaluate} == {response}. Expected: {expected}")

# Tests for individual modules
def device_module():
    __test("device.NAME", 'monocle')
    __test("len(device.mac_address())", 17)
    __test("len(device.VERSION)", 12)
    __test("len(device.GIT_TAG)", 9)
    __test("device.GIT_REPO", 'https://github.com/brilliantlabsAR/monocle-micropython')
    __test("isinstance(device.battery_level(), int)", True)
    __test("device.prevent_sleep(True)", None)
    __test("device.prevent_sleep(False)", None)
    __test("str(device.Storage())", 'Storage(start=0x0006d000, len=602112)')

def display_module():
    # Line: spinning animation
    scale = 300
    x_offset = display.WIDTH // 2
    y_offset = display.HEIGHT // 2
    angle = 0
    while angle <= 2 * math.pi * 10:
        x1 = int(x_offset + math.cos(angle) * scale)
        y1 = int(y_offset + math.sin(angle) * scale)
        x2 = int(x_offset + math.cos(angle + math.pi) * scale)
        y2 = int(y_offset + math.sin(angle + math.pi) * scale)
        line = display.Line(x1, y1, x2, y2, display.WHITE)
        display.show(line)
        angle += math.pi / 20

    # Line: rectangle around the display edges
    t = 10
    h = display.HEIGHT - t
    w = display.WIDTH - t

    display.show(
        display.Line(t, t, t, h, display.WHITE, thickness=t), # left
        display.Line(w, t, w, h, display.WHITE, thickness=t), # right
        display.Line(t, t, w, t, display.WHITE, thickness=t), # top
        display.Line(t, h - 200, w, h - 200, display.WHITE, thickness=t), # bottom
    )

    # Rectangle: growing rectangle, getting larger than the display
    x_offset = display.WIDTH // 2
    y_offset = display.HEIGHT // 2
    for i in range(0, display.WIDTH // 2 + 100, 10):
        x1 = x_offset - i
        y1 = y_offset - i
        x2 = x_offset + i
        y2 = y_offset + i
        display.show(display.Rectangle(x1, y1, x2, y2, display.YELLOW))

    # Rectangle: test with ((x1,y1), (x2,y2)) with x1 > x2 or y1 > y2

    # Test constants
    __test("display.WIDTH", 640)
    __test("display.HEIGHT", 400)
    __test("display.FONT_WIDTH", 32)
    __test("display.FONT_HEIGHT", 400)

def camera_module():
    print("TODO camera module")

def microphone_module():
    print("TODO microphone module")

def touch_module():
    __test("touch.state('A')", False)
    __test("touch.state('B')", False)
    __test("touch.state('BOTH')", False)
    __test("touch.state(touch.A)", False)
    __test("touch.state(touch.B)", False)
    __test("touch.state(touch.BOTH)", False)
    __test("touch.state('B')", False)
    __test("callable(touch.callback)", True)

def led_module():
    __test("led.on('RED')", None)
    __test("led.on(led.RED)", None)
    __test("led.off('RED')", None)
    __test("led.off(led.RED)", None)
    __test("led.on('GREEN')", None)
    __test("led.on(led.GREEN)", None)
    __test("led.off('GREEN')", None)
    __test("led.off(led.GREEN)", None)

def fpga_module():
    # Test read and check the FPGA chip ID at the same time
    __test("fpga.read(0x0001, 3)", b'K\x07\x00')

    # Test writes
    __test("fpga.write(0x0000, b'')", None)
    __test("fpga.write(0x0000, b'done')", None)
    __test("fpga.write(0x0000, b'a' * 255)", None)

    # Ensure that ROM strings can't be sent on the SPI
    __test("fpga.write(0x0000, 'done')", TypeError)

    # Ensure that the min and max transfer sizes are respected
    __test("fpga.read(0x0000, 256), ", ValueError)
    __test("fpga.read(0x0000, 0), ", ValueError)
    __test("fpga.read(0x0000, -1), ", ValueError)
    __test("fpga.write(0x0000, b'a' * 256)", ValueError)

def bluetooth_module():
    __test("bluetooth.connected()", True)
    __test("isinstance(bluetooth.max_length(), int)", True)
    max_length = bluetooth.max_length()
    __test("bluetooth.send(b'')", None)
    __test(f"bluetooth.send(b'a' * {max_length})", None)
    __test(f"bluetooth.send(b'a' * ({max_length} + 1))", ValueError)
    __test("callable(bluetooth.receive_callback)", True)

def time_module():
    # Test setting and checking the time
    __test("time.time(1674252171)", None)
    __test("time.time()", 1674252171)
    __test("time.now()", {'timezone': '00:00', 'weekday': 'friday', 'minute': 2, 'day': 20, 'yearday': 20, 'month': 1, 'second': 51, 'hour': 22, 'year': 2023})

    # Check time dict at a specified epoch
    __test("time.now(1674253104)", {'timezone': '00:00', 'weekday': 'friday', 'minute': 18, 'day': 20, 'yearday': 20, 'month': 1, 'second': 24, 'hour': 22, 'year': 2023})

    # Test timezones
    __test("time.zone('5:30')", None)
    __test("time.zone()", '05:30')
    __test("time.now()", {'timezone': '05:30', 'weekday': 'saturday', 'minute': 32, 'day': 21, 'yearday': 21, 'month': 1, 'second': 51, 'hour': 3, 'year': 2023})
    __test("time.zone('-12:00')", None)
    __test("time.now()", {'timezone': '-12:00', 'weekday': 'friday', 'minute': 2, 'day': 20, 'yearday': 20, 'month': 1, 'second': 51, 'hour': 10, 'year': 2023})

    # Test getting epochs from time dict
    __test("time.mktime({'minute': 18, 'day': 20, 'month': 1, 'second': 24, 'hour': 22, 'year': 2023})", 1674253104)

    # Test sleep
    __test("time.time(1674252171)", None)
    __test("time.time()", 1674252171)
    __test("time.sleep(2)", None)
    __test("time.time()", 1674252173)
    __test("time.sleep(0.25)", None)
    __test("time.time()", 1674252173)

    # Test invalid values
    __test("time.time(-1)", ValueError)
    __test("time.zone('-14:00')", ValueError)
    __test("time.zone('15:00')", ValueError)
    __test("time.zone('-12:30')", ValueError)

def update_module():
    __test("callable(update.micropython)", True)

    __test("update.Fpga.erase()", None)

    # Ensure that ROM strings can't be sent on the SPI
    __test("update.Fpga.write('done')", TypeError)

    # Write and read back value
    __test("update.Fpga.write(b'done')", None)
    __test("update.Fpga.read(0x0000, 4)", b'done')

    # Check that limits of the FPGA app region are respected
    __test("update.Fpga.read(444430, 4)", b'\xff\xff\xff\xff')
    __test("update.Fpga.read(444430, 8)", ValueError)

def all():
    device_module()
    display_module()
    camera_module()
    microphone_module()
    touch_module()
    led_module()
    fpga_module()
    bluetooth_module()
    time_module()
    update_module()
