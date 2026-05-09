# todo

## 1 不同的mask测试任务

新增工业mask的测试案例,需要对`input/mask/Different_mask_tests`目录下T1~T10的mask全部进行测试，使用`validation/golden/src/litho.cpp`输出所有的测试图样，测试光学参数使用`input/config/golden_1024.json`，新建新的批量测试参数`Different_mask_tests.json`

## 2 不同的分辨率测试

对`input/mask/Different_resolution_tests`路径下的mask进行测试，光学参数出了分辨率下使用`input/config/golden_1024.json`，你需要新建对应的，`Different_resolution_tests.json`


完成以上的测试，所有的结果放在一个全新的output文件夹`output`，新建对应的output/Different_mask_tests，output/Different_resolution_tests