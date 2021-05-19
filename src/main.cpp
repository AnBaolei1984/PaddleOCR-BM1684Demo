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

//#include "glog/logging.h"
#include "opencv2/core.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <vector>

#include <cstring>
#include <fstream>
#include <numeric>

#include <include/config.h>
#include <include/ocr_det.h>
#include <include/ocr_rec.h>

using namespace std;
using namespace cv;
using namespace BMPaddleOCR;

int main(int argc, char **argv) {
  if (argc < 3) {
    std::cerr << "[ERROR] usage: " << argv[0]
              << " configure_filepath image_path\n";
    exit(1);
  }

  Config config(argv[1]);
  config.PrintConfigInfo();
  std::string img_path(argv[2]);

  DBDetector det(config.det_bmodel_path, config.device_id, config.max_side_len,
                 config.det_db_thresh, config.det_db_box_thresh,
                 config.det_db_unclip_ratio, config.visualize);

  CRNNRecognizer rec(config.rec_model_dir,
             config.device_id, config.cpu_math_library_num_threads,
             config.char_list_file);

  cv::Mat srcimg = cv::imread(img_path, cv::IMREAD_COLOR, 0);
  auto start = std::chrono::system_clock::now();
  std::vector<std::vector<std::vector<int>>> boxes;
  det.Run(srcimg, boxes);
  auto end = std::chrono::system_clock::now();
  auto duration_det =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  start = std::chrono::system_clock::now();
  rec.Run(boxes, srcimg);
  end = std::chrono::system_clock::now();
  auto duration_rec =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  std::cout << "det cost "
            << double(duration_det.count()) *
                   std::chrono::microseconds::period::num /
                   std::chrono::microseconds::period::den
            << "s" << std::endl;
  std::cout << "rec cost "
            << double(duration_rec.count()) *
                   std::chrono::microseconds::period::num /
                   std::chrono::microseconds::period::den
            << "s" << std::endl;
  std::cout << "total cost "
            << double(duration_rec.count() + duration_det.count()) *
                   std::chrono::microseconds::period::num /
                   std::chrono::microseconds::period::den
            << "s" << std::endl;

  return 0;
}
