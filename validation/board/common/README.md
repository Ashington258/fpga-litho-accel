# 公共验证工具

本目录包含 JTAG 和 PCIe 验证方式的公共工具脚本。

## 📁 文件说明

| 文件                  | 用途                                      |
| --------------------- | ----------------------------------------- |
| `axi_memory_test.tcl` | AXI 内存读写测试脚本，用于验证 DDR 连通性 |

## 🔧 使用方法

### AXI 内存测试

```tcl
# 在 Vivado Tcl Console 中执行
cd e:/fpga-litho-accel/validation/board/common
source axi_memory_test.tcl

# 执行内存测试
run_axi_memory_test
```

测试内容：
- 写入测试数据到指定地址
- 读取并验证数据一致性
- 报告测试结果

## 🔗 相关文档

- [JTAG 验证指南](../jtag/README.md)
- [PCIe 验证指南](../pcie/README.md)