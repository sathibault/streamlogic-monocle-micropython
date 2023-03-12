# Copyright 2023 StreamLogic, LLC.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#
# See the License for the specific language governing permissions and
# limitations under the License.

import __camera
import bluetooth
import fpga
import time

def power_on():
  __camera.power_on()

def start():
    __camera.command(5)

def stop():
    __camera.command(4)

def overlay(enable):
  if enable == True:
    __camera.wake()
    __camera.command(5)
  else:
    __camera.command(4)
    time.sleep_ms(100);
    __camera.sleep()

class Capture:
  def __init__(self, dev, dim, blksize):
    if len(dim) != 3:
      raise Exception('dim must be an array of [xres, yres, bpp]')

    self.addr = dev*256 + 5
    self.total = dim[0] * dim[1] * dim[2]
    self.chunk = (252//dim[2]) * dim[2]
    self.blksize = blksize * dim[2]
    self.pos = None

    if self.total % self.blksize != 0:
      raise Exception('blksize is incompatible with image dimensions')

  def __iter__(self):
    if self.pos != None:
      raise Exception('capture already consumed')
    self.pos = 0
    return self

  def __next__(self):
    if self.pos < self.total:
      buf = fpga.read_strm(self.addr, self.blksize, self.chunk)
      self.pos += len(buf)
      return buf
    raise StopIteration

def capture(dim, blksize, readout_id=0):
  """
  Captures a single image from the camera, of the give dimensions from
  the FB Readout configured for the given blksize.  The dimensions
  should be given as [xres, yres, bpp]
  """

  dev = __camera.readout_dev(readout_id);
  addr = dev*256 + 4
  fpga.write(addr, b'')
  return Capture(dev, dim, blksize)
