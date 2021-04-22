// Copyright (c) 2020 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <include/ocr_det.h>

namespace BMPaddleOCR {
using namespace std;

void DBDetector::Run(cv::Mat &img,
                     std::vector<std::vector<std::vector<int>>> &boxes) {
  float ratio_h{};
  float ratio_w{};
  cv::Mat srcimg;
  cv::Mat resize_img;

  img.copyTo(srcimg);
  this->resize_op_.Run(img, resize_img, this->max_side_len_, ratio_h, ratio_w);
  cv::Mat real_input_img(640, 640, CV_8UC3, cv::Scalar(0, 0, 0));
  resize_img.copyTo(real_input_img(cv::Rect(0,
                                   0, resize_img.cols, resize_img.rows)));
  vector<cv::Mat> inputs;
  inputs.push_back(real_input_img);
  bm_ocr_det_->preprocess(inputs, this->mean_, this->scale_, this->is_scale_);

  vector<float*> results;
  vector<vector<int>> output_shapes;
  bm_ocr_det_->run(output_shapes, results);

  vector<int> output_shape = output_shapes[0];
  int out_num = accumulate(output_shape.begin(), output_shape.end(), 1,
                                multiplies<int>());

  int n2 = output_shape[2];
  int n3 = output_shape[3];
  int n = n2 * n3;

  vector<float> pred(n, 0.0);
  vector<unsigned char> cbuf(n, ' ');
  float* out_data = results[0];
  for (int i = 0; i < n; i++) {
    pred[i] = float(out_data[i]);
    cbuf[i] = (unsigned char)((out_data[i]) * 255);
  }

  cv::Mat cbuf_map(n2, n3, CV_8UC1, (unsigned char *)cbuf.data());
  cv::Mat pred_map(n2, n3, CV_32F, (float *)pred.data());

  const double threshold = this->det_db_thresh_ * 255;
  const double maxvalue = 255;
  cv::Mat bit_map;
  cv::threshold(cbuf_map, bit_map, threshold, maxvalue, cv::THRESH_BINARY);
  cv::Mat dilation_map;
  cv::Mat dila_ele = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2,2));
  cv::dilate(bit_map, dilation_map, dila_ele);
  boxes = post_processor_.BoxesFromBitmap(
      pred_map, dilation_map, this->det_db_box_thresh_, this->det_db_unclip_ratio_);

  boxes = post_processor_.FilterTagDetRes(boxes, ratio_h, ratio_w, srcimg);
  //// visualization
  if (this->visualize_) {
    Utility::VisualizeBboxes(srcimg, boxes);
  }
}

} // namespace PaddleOCR
