# PaddleOCR-BM1684Demo
这个是在比特大陆BM1684系列AI硬件上支持PaddleOCR的demo,可以支持x86(SC5)和arm(SE5/SM5)两类设备。

## 编译

- 编译x86执行 sh tools/build_x86.sh

- 编译arm执行 sh tools/build_arm.sh

编译前，需要在对应脚本中设置两个路径。
### a) BM_DIR设置为比特大陆的bmnnsdk2的目录

### b) 设置LIB_DIR

 paddle的模型在x86上推理是padddle_inference实现，编译padddle_inference请参考（https://www.paddlepaddle.org.cn/documentation/docs/zh/guides/05_inference_deployment/inference/build_and_install_lib_cn.html#congyuanmabianyi）。

在arm上，paddle模型的推理是基于Paddle-Lite来实现。我们提供了编译好的PaddleLite预测库（链接：https://pan.baidu.com/s/1dyy7VA3sdGLD7VZ3qJZclg 
提取码：1234）


![1941616380438_ pic_hd](https://user-images.githubusercontent.com/49897975/111933306-1e50c900-8afa-11eb-9c8c-9e1f5e4480b0.jpg)



## 运行

sh tools/run.sh

可以看到运行结果

![1821615381682_ pic_hd](https://user-images.githubusercontent.com/49897975/110634280-c2766e00-81e4-11eb-9517-a0d496911566.jpg)
![ocr_vis](https://user-images.githubusercontent.com/49897975/110634314-cacea900-81e4-11eb-978a-0053cba017bc.png)

