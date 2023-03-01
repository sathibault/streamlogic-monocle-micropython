#ifndef FPGA_H
#define FPGA_H

typedef enum {
  camera_feat = 0,
  framebuf_feat = 1,
  fb_out0_feat = 2,
  fb_out1_feat = 3,
  readout_feat = 4,
  graphics_ov_feat = 5,
  text_ov_feat = 6
} fpga_feat_t;

#define MAX_FEATURES 7

extern uint8_t fpga_feature_addrs[];
extern void fpga_discovery();
extern bool fpga_has_feature(fpga_feat_t feat);
extern uint8_t fpga_feature_dev(fpga_feat_t feat);

#endif
