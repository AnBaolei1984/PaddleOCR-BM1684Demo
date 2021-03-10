OPENCV_DIR=/home/bitmain/anbl/git2/PaddleOCR/deploy/cpp_infer/opencv-3.4.7/install
LIB_DIR=/home/bitmain/anbl/git2/Paddle/build/paddle_inference_install_dir
BM_DIR=/home/bitmain/anbl/fae/bmnnsdk2-bm1684_v2.3.0

BUILD_DIR=build
rm -rf ${BUILD_DIR}
mkdir ${BUILD_DIR}
cd ${BUILD_DIR}
cmake .. \
    -DPADDLE_LIB=${LIB_DIR} \
    -DWITH_STATIC_LIB=ON \
    -DBM_LIB=${BM_DIR} \
    -DOPENCV_DIR=${OPENCV_DIR}

make -j
