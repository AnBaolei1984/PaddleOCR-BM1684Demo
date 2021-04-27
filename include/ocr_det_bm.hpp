//
//  ocr_det_bm.hpp
//  BMPaddleOCR
//
//  Created by Bitmain on 2021/2/25.
//  Copyright © 2021年 AnBaolei. All rights reserved.
//

#ifndef ocr_det_bm_hpp
#define ocr_det_bm_hpp

#include <cstring>
#include <memory>
#include "opencv2/core.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "bmodel_base.hpp"

namespace BMPaddleOCR {

class BMOCRDet : public BmodelBase{

public:
  BMOCRDet(const std::string bmodel, int device_id);
  ~BMOCRDet();

  bool preprocess(std::vector<cv::Mat>& in_img,
            const std::vector<float>& mean,
            const std::vector<float>& scale, bool is_scale = true);

  bool run(std::vector<std::vector<int>>& output_shapes,
            std::vector<float*>& results);

};
} // namespace BMPaddleOCR

#endif /* ocr_det_bm_hpp */
