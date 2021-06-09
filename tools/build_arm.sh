LIB_DIR=/home/bitmain/anbl/fae/bmnnsdk2-bm1684_v2.3.2/examples/YOLOv3_object/PaddleOCR-CppDemo/Paddle_Lite_libs
BM_DIR=/home/bitmain/anbl/fae/bmnnsdk2-bm1684_v2.3.2

BUILD_DIR=build
rm -rf ${BUILD_DIR}
mkdir ${BUILD_DIR}
cd ${BUILD_DIR}
cmake .. \
    -DPADDLE_LIB=${LIB_DIR} \
    -DWITH_STATIC_LIB=ON \
    -DSOC_MODE=ON \
    -DBM_LIB=${BM_DIR} \

make -j
