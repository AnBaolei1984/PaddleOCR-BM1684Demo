//
//  ocr_rec_bm.hpp
//  BMPaddleOCR
//
//  Created by Bitmain on 2021/4/23.
//  Copyright © 2021年 AnBaolei. All rights reserved.
//

#ifndef ocr_rec_bm_hpp
#define ocr_rec_bm_hpp

#include <cstring>
#include <memory>
#include "opencv2/core.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "bmodel_base.hpp"

namespace BMPaddleOCR {

class BMOCRRec : public BmodelBase{

public:
  BMOCRRec(const std::string bmodel, int device_id);
  ~BMOCRRec();

  bool preprocess(std::vector<cv::Mat>& in_img,
            const std::vector<float>& mean,
            const std::vector<float>& scale, bool is_scale = true);

  bool run(std::vector<std::vector<int>>& output_shapes,
            std::vector<float*>& results);
private:
  int input_width_;
  float* result_ = nullptr;
};
} // namespace BMPaddleOCR

#endif /* ocr_rec_bm_hpp */
