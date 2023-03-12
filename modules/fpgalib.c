/*

Copyright 2023 StreamLogic, LLC.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#include "monocle.h"
#include "fpgalib.h"

// Dyamically discovered features available on FPGA
uint8_t fpga_feature_addrs[MAX_FEATURES] = {0};


static bool fpga_test_api(uint8_t *addr, uint8_t *api) {
  uint8_t status;

  spi_write(FPGA, addr, 2, true /* hold CS for read */);
  spi_read(FPGA, &status, 1);
  *api = status >> 5;

  return (status & 0x10) != 0;
}

static bool discovered = false;

void fpga_discovery() {
  uint8_t api;
  uint8_t addr[2] = {0, 0};

  if (discovered) return;

  // cameras
  addr[0] = 0x10;
  if (fpga_test_api(addr, &api) && api == 0)
    fpga_feature_addrs[camera_feat] = 0x10;

  // frame buffers
  addr[0] = 0x20;
  if (fpga_test_api(addr, &api) && api == 0) {
    fpga_feature_addrs[framebuf0_feat] = addr[0]++;
    if (fpga_test_api(addr, &api) && api == 0)
      fpga_feature_addrs[framebuf1_feat] = addr[0];
  }

  // frame buffer out
  addr[0] = 0x30;
  if (fpga_test_api(addr, &api) && api == 0) {
    fpga_feature_addrs[fb_out0_feat] = addr[0]++;
    if (fpga_test_api(addr, &api) && api == 0)
      fpga_feature_addrs[fb_out1_feat] = addr[0];
  }

  // overlays
  addr[0] = 0x44;
  while (fpga_test_api(addr, &api)) {
    switch (api) {
    case 0:
      fpga_feature_addrs[graphics_ov_feat] = addr[0];
      break;
    case 1:
      fpga_feature_addrs[text_ov_feat] = addr[0];
      break;
    }
    addr[0] += 1;
  }

  // readouts
  addr[0] = 0x50;
  if (fpga_test_api(addr, &api) && api == 0) {
    fpga_feature_addrs[readout0_feat] = addr[0]++;
    if (fpga_test_api(addr, &api) && api == 0)
      fpga_feature_addrs[readout1_feat] = addr[0];
  }
  discovered = true;
}

bool fpga_has_feature(fpga_feat_t feat) {
  fpga_discovery();
  return fpga_feature_addrs[feat] != 0;
}

uint8_t fpga_feature_dev(fpga_feat_t feat) {
  fpga_discovery();
  return fpga_feature_addrs[feat];
}

uint8_t fpga_graphics_dev() {
  return fpga_feature_dev(graphics_ov_feat);
}

void fpga_write_internal(uint8_t *buf, unsigned int len, bool hold) {
  spi_write(FPGA, buf, len, hold);
}
