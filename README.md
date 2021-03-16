# PaddleOCR-CppDemo
这个是在比特大陆BM1684系列AI硬件上支持PaddleOCR的demo

1. 配置依赖库路径

LIB_DIR=/project/Paddle_Lite_libs/  
BM_DIR=/workspace/

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

其中分别设置：
LIB_DIR为paddle的库路径  
BM_DIR为比特大陆bmnnsdk2的路径。

2. 编译

执行 sh tools/build.sh

3. 运行

sh tools/run.sh

可以看到运行结果

![1821615381682_ pic_hd](https://user-images.githubusercontent.com/49897975/110634280-c2766e00-81e4-11eb-9517-a0d496911566.jpg)
![ocr_vis](https://user-images.githubusercontent.com/49897975/110634314-cacea900-81e4-11eb-978a-0053cba017bc.png)

