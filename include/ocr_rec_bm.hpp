//
//  ocr_rec_bm.hpp
//  BMPaddleOCR
//
//  Created by Bitmain on 2021/2/25.
//  Copyright © 2021年 AnBaolei. All rights reserved.
//

#ifndef ocr_rec_bm_hpp
#define ocr_rec_bm_hpp

#include <cstring>
#include <memory>
#include "bmodel_base.hpp"

namespace BMPaddleOCR {

class BMOCRRec : public BmodelBase{

public:
  BMOCRRec(const std::string bmodel, int device_id);
  ~BMOCRRec();

  bool run(const std::vector<float*>& inputs,
            std::vector<std::vector<int>>& output_shapes,
            std::vector<float*>& results);
};
} // namespace BMPaddleOCR

#endif /* ocr_rec_bm_hpp */
