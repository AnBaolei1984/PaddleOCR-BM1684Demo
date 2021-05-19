//
//  ocr_rec_bm.cpp
//  BMPaddleOCR
//
//  Created by Bitmain on 2021/2/25.
//  Copyright © 2021年 AnBaolei. All rights reserved.
//

#include <include/ocr_rec_bm.hpp>
#include <vector>
#include <numeric>

namespace BMPaddleOCR {

using namespace std;
using namespace cv;

BMOCRRec::BMOCRRec(const std::string bmodel, int device_id) {
  device_id_ = device_id;
  bmodel_path_ = bmodel;
  load_model();

  bm_status_t ret = bm_image_create_batch(bm_handle_, net_h_,
                        net_w_,
                        FORMAT_BGR_PLANAR,
                        data_type_,
                        scaled_inputs_, batch_size_);
  if (BM_SUCCESS != ret) {
    std::cerr << "ERROR: bm_image_create_batch failed" << std::endl;
    exit(-1);
  }
}

BMOCRRec::~BMOCRRec() {
  if (result_ != nullptr) {
    delete []result_;
  }
}

bool BMOCRRec::preprocess(std::vector<cv::Mat>& in_img,
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

  bm_image raw_img;
  bmcv::uploadMat(in_img[0]);
  cv::bmcv::toBMI(in_img[0], &raw_img, false);

  bm_image processed_img;
  bm_image_create(bm_handle_, in_img[0].rows, in_img[0].cols,
             FORMAT_BGR_PLANAR, DATA_TYPE_EXT_1N_BYTE, &processed_img, NULL);
  bmcv_rect_t crop_rect = {0, 0, in_img[0].cols, in_img[0].rows};
  bmcv_image_vpp_convert(bm_handle_, 1,
                       raw_img, &processed_img, &crop_rect);
  bm_image scale_img;
  bm_image_create(bm_handle_, in_img[0].rows, in_img[0].cols,
             FORMAT_BGR_PLANAR, data_type_, &scale_img, NULL);
  bmcv_image_convert_to(bm_handle_, batch_size_,
             convert_attr, &processed_img, &scale_img);
  bm_image_destroy(processed_img);
  bm_image_destroy(raw_img);

  bmcv_copy_to_atrr_t copy_to_attr;
  copy_to_attr.start_x = 0;
  copy_to_attr.start_y = 0;
  copy_to_attr.padding_r = 0;
  copy_to_attr.padding_g = 0;
  copy_to_attr.padding_b = 0;
  copy_to_attr.if_padding = 1;

  bmcv_image_copy_to(bm_handle_, copy_to_attr,
                            scale_img, scaled_inputs_[0]);

  bm_image_destroy(scale_img);
  input_width_ = in_img[0].cols;
  return true;
}

bool BMOCRRec::run(std::vector<std::vector<int>>& output_shapes,
                        vector<float*>& results) {
  forward();
  vector<int> output_shape(output_shapes_[0]);
  output_shape[3] = round(input_width_ / 4);
  output_shapes.push_back(output_shape);
  int out_size = std::accumulate(output_shape.begin(), output_shape.end(), 1,
                                  std::multiplies<int>());
  if (result_ != nullptr) {
    delete []result_;
  }
  result_ = new float[out_size];
  float* output = reinterpret_cast<float*>(outputs_[0]);
  for (int i = 0; i < output_shape[0]; i++) {
    float* src_n = output +
        i * output_shapes_[0][1] * output_shapes_[0][2] * output_shapes_[0][3];
    float* dst_n = result_ +
        i * output_shape[1] * output_shape[2] * output_shape[3];
    for (int j = 0; j < output_shape[1]; j++) {
      float* src_c = src_n +
              j * output_shapes_[0][2] * output_shapes_[0][3];
      float* dst_c = dst_n +
              j * output_shape[2] * output_shape[3];
      for (int k = 0; k < output_shape[2]; k++) {
        float* src_h = src_c + k * output_shapes_[0][3];
        float* dst_h = dst_c + k * output_shape[3];
        for (int l = 0; l < output_shape[3]; l++) {
          dst_h[l] = src_h[l];
        }
      }
    }
  }
  results.push_back(result_);
  return true;
}
} // namespace BMPaddleOCR
