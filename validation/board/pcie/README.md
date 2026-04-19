# PCIe DMA 板级验证指南

> **状态**：🔲 待开发
> 
> 本目录用于 PCIe DMA 高速数据传输验证，适合大规模生产验证场景。

## 🎯 目标特性

| 特性         | 目标值                         |
| ------------ | ------------------------------ |
| 数据传输速度 | ~秒级（1MB数据）               |
| Host 接口    | PCIe Gen3 x8                   |
| DMA引擎      | Xilinx XDMA / AXI DMA          |
| 软件栈       | Linux Kernel Driver + User API |

## 📋 开发计划

### Phase 1: DMA IP 集成
- [ ] 添加 Xilinx XDMA IP 到 Block Design
- [ ] 配置 PCIe 接口和 DMA 通道
- [ ] 连接 DMA 到 DDR Memory Controller

### Phase 2: Host 驱动开发
- [ ] Linux Kernel DMA Driver
- [ ] User-space API (C/C++ library)
- [ ] Python binding for testing

### Phase 3: 验证流程集成
- [ ] 数据加载自动化脚本
- [ ] HLS IP 控制接口
- [ ] 结果回读和分析

## 📁 预期目录结构

```
pcie/
├── README.md           # 本说明文件
├── driver/             # Host 驱动程序
│   ├── kernel/         # Linux Kernel Driver
│   └── lib/            # User-space Library
├── scripts/            # 验证脚本
│   ├── load_data.py    # 数据加载脚本
│   └── run_hls.py      # HLS 执行脚本
└── data/               # 验证数据（二进制格式）
```

## 🔗 参考资料

- [Xilinx DMA Subsystem](https://docs.xilinx.com/r/en-US/pg195-dma-subsystem-for-pcie)
- [AXI DMA Reference](https://docs.xilinx.com/r/en-US/pg021-axi-dma)

---

**当前推荐**：开发阶段请使用 `../jtag/` 目录的 JTAG 验证方式。