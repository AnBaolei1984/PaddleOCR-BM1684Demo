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

#include <include/ocr_rec.h>
#include "paddle_use_kernels.h"
#include "paddle_use_ops.h"

namespace BMPaddleOCR {

void CRNNRecognizer::Run(std::vector<std::vector<std::vector<int>>> boxes,
                         cv::Mat &img) {
  cv::Mat srcimg;
  img.copyTo(srcimg);
  cv::Mat crop_img;
  cv::Mat resize_img;

  std::cout << "The predicted text is :" << std::endl;
  int index = 0;
  for (int i = boxes.size() - 1; i >= 0; i--) {
    crop_img = GetRotateCropImage(srcimg, boxes[i]);
    float wh_ratio = float(crop_img.cols) / float(crop_img.rows);
    this->resize_op_.Run(crop_img, resize_img, wh_ratio);

    std::vector<float> input(1 * 3 * resize_img.rows * resize_img.cols, 0.0f);
    bm_preprocess_->Permute_Normalize(resize_img, this->mean_, this->scale_,
                            this->is_scale_, input.data());
  
    auto input_names = this->predictor_->GetInputNames();
    //auto start = std::chrono::system_clock::now();
#ifndef SOC_MODE
    auto input_t = this->predictor_->GetInputTensor(input_names[0]);
    input_t->Reshape({1, 3, resize_img.rows, resize_img.cols});
    input_t->copy_from_cpu(input.data());
    this->predictor_->ZeroCopyRun();
#else
    auto input_t = this->predictor_->GetInput(0);
    input_t->Resize({1, 3, resize_img.rows, resize_img.cols});
    input_t->CopyFromCpu(input.data());
    this->predictor_->Run();
#endif
   // auto end = std::chrono::system_clock::now();
    //auto duration =
      //std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    //std::cout << " cost "
      //      << double(duration.count()) *
        //           std::chrono::microseconds::period::num /
          //         std::chrono::microseconds::period::den
          //  << "s" << std::endl;
 
    std::vector<int64_t> rec_idx;
    auto output_names = this->predictor_->GetOutputNames();
#ifndef SOC_MODE
    auto output_t = this->predictor_->GetOutputTensor(output_names[0]);
    auto output_t_1 = this->predictor_->GetOutputTensor(output_names[1]);
#else
    auto output_t = this->predictor_->GetOutput(0);
    auto output_t_1 = this->predictor_->GetOutput(1);
#endif
    auto rec_idx_lod = output_t->lod();
    auto shape_out = output_t->shape();
    int out_num = std::accumulate(shape_out.begin(), shape_out.end(), 1,
                                  std::multiplies<int>());

    rec_idx.resize(out_num);
#ifndef SOC_MODE
    output_t->copy_to_cpu(rec_idx.data());
#else
    auto* out_ptr = output_t->mutable_data<long>();
    for (int j = 0; j < out_num; j++) {
      rec_idx[j] = out_ptr[j];
    }
    //output_t->CopyToCpu(rec_idx.data());
#endif

    std::vector<int> pred_idx;
    for (int n = int(rec_idx_lod[0][0]); n < int(rec_idx_lod[0][1]); n++) {
      pred_idx.push_back(int(rec_idx[n]));
    }

    if (pred_idx.size() < 1e-3)
      continue;

    index += 1;
    std::cout << index << "\t";
    for (int n = 0; n < pred_idx.size(); n++) {
      std::cout << label_list_[pred_idx[n]];
    }

    std::vector<float> predict_batch;
    auto predict_lod = output_t_1->lod();
    auto predict_shape = output_t_1->shape();
    int out_num_1 = std::accumulate(predict_shape.begin(), predict_shape.end(),
                                    1, std::multiplies<int>());

    predict_batch.resize(out_num_1);
#ifndef SOC_MODE
    output_t_1->copy_to_cpu(predict_batch.data());
#else
    output_t_1->CopyToCpu(predict_batch.data());
#endif
    int argmax_idx;
    int blank = predict_shape[1];
    float score = 0.f;
    int count = 0;
    float max_value = 0.0f;

    for (int n = predict_lod[0][0]; n < predict_lod[0][1] - 1; n++) {
      argmax_idx =
          int(Utility::argmax(&predict_batch[n * predict_shape[1]],
                              &predict_batch[(n + 1) * predict_shape[1]]));
      max_value =
          float(*std::max_element(&predict_batch[n * predict_shape[1]],
                                  &predict_batch[(n + 1) * predict_shape[1]]));
      if (blank - 1 - argmax_idx > 1e-5) {
        score += max_value;
        count += 1;
      }
    }
    score /= count;
    std::cout << "\tscore: " << score << std::endl;
  }
}

void CRNNRecognizer::LoadModel(const std::string &model_dir) {
#ifndef SOC_MODE
  AnalysisConfig config;
  config.SetModel(model_dir + "/model", model_dir + "/params");
#else
  CxxConfig config;
  config.set_model_file(model_dir + "/model");
  config.set_param_file(model_dir + "/params");
  std::vector<Place> valid_places{Place{TARGET(kARM), PRECISION(kFloat)}};
  valid_places.push_back(Place{TARGET(kARM), PRECISION(kInt64)});
  valid_places.push_back(Place{TARGET(kARM), PRECISION(kInt32)});
  config.set_valid_places(valid_places);
#endif
#ifndef SOC_MODE
  config.DisableGpu();
  config.SetCpuMathLibraryNumThreads(this->cpu_math_library_num_threads_);
  config.SwitchUseFeedFetchOps(false);
  config.SwitchSpecifyInputNames(true);
  config.SwitchIrOptim(true);
  config.EnableMemoryOptim();
  config.DisableGlogInfo();
#else
  config.set_threads(this->cpu_math_library_num_threads_);
#endif

  this->predictor_ = CreatePaddlePredictor(config);
#ifdef SOC_MODE
  predictor_->SaveOptimizedModel(".",
                                LiteModelType::kNaiveBuffer);
#endif
}

cv::Mat CRNNRecognizer::GetRotateCropImage(const cv::Mat &srcimage,
                                           std::vector<std::vector<int>> box) {
  cv::Mat image;
  srcimage.copyTo(image);
  std::vector<std::vector<int>> points = box;

  int x_collect[4] = {box[0][0], box[1][0], box[2][0], box[3][0]};
  int y_collect[4] = {box[0][1], box[1][1], box[2][1], box[3][1]};
  int left = int(*std::min_element(x_collect, x_collect + 4));
  int right = int(*std::max_element(x_collect, x_collect + 4));
  int top = int(*std::min_element(y_collect, y_collect + 4));
  int bottom = int(*std::max_element(y_collect, y_collect + 4));

  cv::Mat img_crop;
  image(cv::Rect(left, top, right - left, bottom - top)).copyTo(img_crop);

  for (int i = 0; i < points.size(); i++) {
    points[i][0] -= left;
    points[i][1] -= top;
  }

  int img_crop_width = int(sqrt(pow(points[0][0] - points[1][0], 2) +
                                pow(points[0][1] - points[1][1], 2)));
  int img_crop_height = int(sqrt(pow(points[0][0] - points[3][0], 2) +
                                 pow(points[0][1] - points[3][1], 2)));

  cv::Point2f pts_std[4];
  pts_std[0] = cv::Point2f(0., 0.);
  pts_std[1] = cv::Point2f(img_crop_width, 0.);
  pts_std[2] = cv::Point2f(img_crop_width, img_crop_height);
  pts_std[3] = cv::Point2f(0.f, img_crop_height);

  cv::Point2f pointsf[4];
  pointsf[0] = cv::Point2f(points[0][0], points[0][1]);
  pointsf[1] = cv::Point2f(points[1][0], points[1][1]);
  pointsf[2] = cv::Point2f(points[2][0], points[2][1]);
  pointsf[3] = cv::Point2f(points[3][0], points[3][1]);

  cv::Mat M = cv::getPerspectiveTransform(pointsf, pts_std);

  cv::Mat dst_img;
  cv::warpPerspective(img_crop, dst_img, M,
                      cv::Size(img_crop_width, img_crop_height),
                      cv::BORDER_REPLICATE);

  if (float(dst_img.rows) >= float(dst_img.cols) * 1.5) {
    cv::Mat srcCopy = cv::Mat(dst_img.rows, dst_img.cols, dst_img.depth());
    cv::transpose(dst_img, srcCopy);
    cv::flip(srcCopy, srcCopy, 0);
    return srcCopy;
  } else {
    return dst_img;
  }
}

} // namespace PaddleOCR
