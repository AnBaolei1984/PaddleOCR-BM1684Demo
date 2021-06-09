LIB_DIR=/home/bitmain/anbl/git2/Paddle/build/paddle_inference_install_dir
BM_DIR=/home/bitmain/anbl/fae/bmnnsdk2-bm1684_v2.3.2

BUILD_DIR=build
rm -rf ${BUILD_DIR}
mkdir ${BUILD_DIR}
cd ${BUILD_DIR}
cmake .. \
    -DPADDLE_LIB=${LIB_DIR} \
    -DWITH_STATIC_LIB=OFF \
    -DBM_LIB=${BM_DIR} \

make -j
