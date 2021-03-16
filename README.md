# PaddleOCR-CppDemo
这个是在比特大陆BM1684系列AI硬件上支持PaddleOCR的demo,可以支持x86和arm两种设备

## 配置依赖库路径
### x86 配置如下
![1615867224(1)](https://user-images.githubusercontent.com/19307549/111254095-48fcd680-864f-11eb-8fc1-eda17535b97a.jpg)


### arm 配置如下
![1615867408(1)](https://user-images.githubusercontent.com/19307549/111254298-b3ae1200-864f-11eb-95a8-3afcca7f8123.jpg)



- **其中分别设置**:  
    - **LIB_DIR为paddle的库路径**
    - **BM_DIR为比特大陆bmnnsdk2的路径**
  

## 编译

执行 sh tools/build.sh

## 运行

sh tools/run.sh

可以看到运行结果

![1821615381682_ pic_hd](https://user-images.githubusercontent.com/49897975/110634280-c2766e00-81e4-11eb-9517-a0d496911566.jpg)
![ocr_vis](https://user-images.githubusercontent.com/49897975/110634314-cacea900-81e4-11eb-978a-0053cba017bc.png)

