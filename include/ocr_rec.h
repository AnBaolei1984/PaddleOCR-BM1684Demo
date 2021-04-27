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

#include "opencv2/core.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "paddle_api.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <vector>

#include <cstring>
#include <fstream>
#include <numeric>

#include <include/postprocess_op.h>
#include <include/preprocess_op.h>
#include <include/utility.h>
#include <include/ocr_rec_bm.hpp>

#ifndef SOC_MODE
#include "paddle_inference_api.h"
#else
using namespace paddle::lite_api;
#endif

namespace BMPaddleOCR {

class CRNNRecognizer {
public:
  explicit CRNNRecognizer(const std::string &model_dir,
                          const int &device_id,
                          const int &cpu_math_library_num_threads,
                          const string &label_path) {
    this->device_id_ = device_id;
    this->cpu_math_library_num_threads_ = cpu_math_library_num_threads;
    this->label_list_ = Utility::ReadDict(label_path);
    this->label_list_.push_back(" ");

    std::string bmodel_path = model_dir + "/rec_cnn.bmodel";
    std::shared_ptr<BMOCRRec> rec_ptr(new BMOCRRec(bmodel_path, device_id));
    sp_rec_ptr_ = rec_ptr;
    bm_ocr_rec_ = sp_rec_ptr_.get();

    LoadModel(model_dir);
  }

  // Load Paddle inference model
  void LoadModel(const std::string &model_dir);
  void Run(std::vector<std::vector<std::vector<int>>> boxes, cv::Mat &img);

private:
  std::shared_ptr<PaddlePredictor> predictor_;

  int device_id_ = 0;
  int cpu_math_library_num_threads_ = 4;
  std::vector<std::string> label_list_;

  std::vector<float> mean_ = {0.5f, 0.5f, 0.5f};
  std::vector<float> scale_ = {1 / 0.5f, 1 / 0.5f, 1 / 0.5f};
  bool is_scale_ = true;

  // pre-process
  CrnnResizeImg resize_op_;
  Normalize normalize_op_;
  Permute permute_op_;

  BMOCRRec* bm_ocr_rec_;
  std::shared_ptr<BMOCRRec> sp_rec_ptr_;

  // post-process
  PostProcessor post_processor_;
  cv::Mat GetRotateCropImage(const cv::Mat &srcimage,
                             std::vector<std::vector<int>> box);

}; // class CrnnRecognizer

} // namespace PaddleOCR
