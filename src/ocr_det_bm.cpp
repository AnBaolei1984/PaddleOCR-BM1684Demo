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
using namespace cv;

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


bool BMOCRDet::preprocess(std::vector<cv::Mat>& in_img,
            const std::vector<float>& mean,
            const std::vector<float>& scale, bool is_scale) {

  double e = 1.0;
  if (is_scale) {
    e /= 255.0;
  }
  bmcv_convert_to_attr convert_attr;
  convert_attr.alpha_0 = e * scale[0];
  convert_attr.beta_0 = -mean[0] * scale[0];
  convert_attr.alpha_1 = e * scale[1];
  convert_attr.beta_1 = -mean[1] * scale[1];
  convert_attr.alpha_2 = e * scale[2];
  convert_attr.beta_2 = -mean[2] * scale[2];

  for (size_t i = 0; i < in_img.size(); i++) {
    bm_image raw_img;
    bmcv::uploadMat(in_img[i]);
    cv::bmcv::toBMI(in_img[i], &raw_img, false);

    bm_image processed_img;
    bm_image_create(bm_handle_, net_h_, net_w_,
             FORMAT_BGR_PLANAR, DATA_TYPE_EXT_1N_BYTE, &processed_img, NULL);
    bmcv_rect_t crop_rect = {0, 0, in_img[i].cols, in_img[i].rows};
    bmcv_image_vpp_convert(bm_handle_, 1,
                       raw_img, &processed_img, &crop_rect);
    bmcv_image_convert_to(bm_handle_, batch_size_,
             convert_attr, &processed_img, scaled_inputs_);
    bm_image_destroy(processed_img);
    bm_image_destroy(raw_img);
  }
  return true;
}

bool BMOCRDet::run(std::vector<std::vector<int>>& output_shapes,
                        vector<float*>& results) {
  forward();
  vector<int> output_shape(output_shapes_[0]);
  output_shapes.push_back(output_shape);
  results.push_back(reinterpret_cast<float*>(outputs_[0]));

  return true;
}
} // namespace BMPaddleOCR
