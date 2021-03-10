//
//  ocr_det_bm.cpp
//  BMPaddleOCR
//
//  Created by Bitmain on 2021/2/25.
//  Copyright © 2021年 AnBaolei. All rights reserved.
//

#include <include/ocr_det_bm.hpp>

namespace BMPaddleOCR {

using namespace std;

BMOCRDet::BMOCRDet(const std::string bmodel, int device_id) {
  device_id_ = device_id;
  bmodel_path_ = bmodel;
  load_model();

  bm_status_t ret = bm_image_create_batch(bm_handle_, net_h_, net_w_,
                        FORMAT_BGR_PLANAR,
                        data_type_,
                        scaled_inputs_, batch_size_);
  if (BM_SUCCESS != ret) {
    std::cerr << "ERROR: bm_image_create_batch failed" << std::endl;
    exit(-1);
  }
}

BMOCRDet::~BMOCRDet() {
}

bool BMOCRDet::run(const vector<float*>& inputs,
                        std::vector<std::vector<int>>& output_shapes,
                        vector<float*>& results) {
  bm_image_copy_host_to_device(scaled_inputs_[0], (void**)&inputs[0]);
  forward();

  vector<int> output_shape(output_shapes_[0]);
  output_shapes.push_back(output_shape);
  results.push_back(reinterpret_cast<float*>(outputs_[0]));

  return true;
}
} // namespace BMPaddleOCR
