## 0. 简介

本文档包含关于使用 [Xilinx FPGA 开发板 Kintex ku3p ultra scale + 开发板 XCKU3P-FFVB676](https://www.ebay.com/itm/167626831054) 的笔记。该板卡品牌为 _阿里云_，型号为 AS02MC04。

通过 JTAG 读取 _Device DNA_ 并使用 [AMD Device Lookup](https://2dbarcode.xilinx.com/scan) 查询结果如下：
<table>
  <tr><td><b>器件</b></td><td>XCKU3P</td></tr>
  <tr><td><b>封装-引脚</b></td><td>FFVB676</td></tr>
  <tr><td><b>版本代码</b></td><td>AAZ</td></tr>
  <tr><td><b>速度等级</b></td><td>1E</td></tr>
</table>

## 1. 参考信息

关于 AS02MC04 信息搜索结果。

## 1.1. GitHub 项目 as02mc04_hack

[as02mc04_hack](https://github.com/TiferKing/as02mc04_hack) 是一个包含部分板卡文件的 GitHub 项目。

项目中指定的器件是 `xcku3p-ffvb676-2-e`，这是一个比通过 DNA 读取的 1E 更快的速度等级 2E。

该项目 readme 的翻译标题为 _AS02MC04 板卡逆向工程数据_，这可能是项目中速度等级错误的原因。不确定不同板卡是否安装了不同速度等级的器件。

## 1.2. _多米果_ 的教程

- [低成本、高性能 FPGA 开发工具：小阿里板 AS02MC04 完整开发教程【第一部分】- 探索 Xilinx Kintex UltraScale+ FPGA 的无限潜力](https://zhuanlan.zhihu.com/p/24657498989) 包含一些引脚映射表的图片。
- [小阿里板 AS02MC04 开发教程【第二部分】- 时钟和 LED 配置以及 Flash 加载配置](https://zhuanlan.zhihu.com/p/25813982411) 包含一些约束文件文本。QSPI 被标识为 MT25QU256，这与 [XCKU5P PCIE 3.0 QSFP X2 笔记](https://gist.github.com/Chester-Gillon/ba675c6ab4e5eb7271f43f8ce4aedb6c) 中使用的基本型号相同。
- [小阿里板 AS02MC04 开发教程【第三部分】- PCIE 3.0 x8 接口开发及速度测试验证](https://zhuanlan.zhihu.com/p/26890429797) 不包含额外的引脚信息。

约束文件副本如下：
```tcl
set_property PACKAGE_PIN B11 [get_ports {LED[0]}]
set_property PACKAGE_PIN C11 [get_ports {LED[1]}]
set_property PACKAGE_PIN A10 [get_ports {LED[2]}]
set_property PACKAGE_PIN B10 [get_ports {LED[3]}]
set_property PACKAGE_PIN E18 [get_ports diff_100mhz_clk_p]
set_property IOSTANDARD LVDS [get_ports diff_100mhz_clk_p]
set_property IOSTANDARD LVDS [get_ports diff_100mhz_clk_n]
set_property PACKAGE_PIN A12 [get_ports LED_G]
set_property PACKAGE_PIN A13 [get_ports LED_R]
set_property PACKAGE_PIN B9 [get_ports LED_HEART]
set_property PACKAGE_PIN B12 [get_ports SFP_1_LED]
set_property PACKAGE_PIN C12 [get_ports SFP_2_LED]
#set_property PACKAGE_PIN K7 [get_ports sfp_mgt_clk_p]
set_property PACKAGE_PIN F12 [get_ports SW_RESET]
set_property IOSTANDARD LVCMOS18 [get_ports {LED[3]}]
set_property IOSTANDARD LVCMOS18 [get_ports {LED[2]}]
set_property IOSTANDARD LVCMOS18 [get_ports {LED[1]}]
set_property IOSTANDARD LVCMOS18 [get_ports {LED[0]}]
set_property IOSTANDARD LVCMOS18 [get_ports LED_G]
set_property IOSTANDARD LVCMOS18 [get_ports LED_HEART]
set_property IOSTANDARD LVCMOS18 [get_ports LED_R]
set_property IOSTANDARD LVCMOS18 [get_ports SW_RESET]
set_property IOSTANDARD LVCMOS18 [get_ports SFP_1_LED]
set_property IOSTANDARD LVCMOS18 [get_ports SFP_2_LED]

set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
set_property BITSTREAM.CONFIG.SPI_BUSWIDTH 4 [current_design]
set_property CONFIG_MODE SPIx4 [current_design]
set_property BITSTREAM.CONFIG.CONFIGRATE 31.9 [current_design]
```

## 2. QSPI

QSPI 使用的 Micron _FBGA Code_ 为 `RW170`，型号为 [MT25QU256ABA8ESF-0SIT](https://www.micron.com/products/storage/nor-flash/serial-nor/part-catalog/part-detail/mt25qu256aba8esf-0sit)

通过 FPGA 设计解析配置比特流：
```
$ xilinx_quad_spi/quad_spi_flasher 
Opening device 0000:01:00.0 (10ee:9038) with IOMMU group 0
Enabled bus master for 0000:01:00.0
Warning: Device device 0000:01:00.0 (10ee:9038) has reduced bandwidth
         Max width x8 speed 8 GT/s. Negotiated width x8 speed 2.5 GT/s

Displaying information for SPI flash using AS02MC04_dma_stream_crc64 design in PCI device 0000:01:00.0 IOMMU group 0
FIFO depth=256
Flash device : Micron N25Q256A
Manufacturer ID=0x20  Memory Interface Type=0xbb  Density=0x19
Flash Size Bytes=33554432  Page Size Bytes=256  Num Address Bytes=4
Successfully parsed bitstream of length 12603672 bytes with 159333 configuration packets
Read 12615680 bytes from SPI flash starting at address 0
Sync word at byte index 0x50
  Type 1 packet opcode NOP
  Type 1 packet opcode write register BSPI words 0000066C
  Type 1 packet opcode write register CMD command BSPI_READ
  Type 1 packet opcode NOP (2 consecutive)
  Type 1 packet opcode write register TIMER words 00000000
  Type 1 packet opcode write register WBSTAR words 00000000
  Type 1 packet opcode write register CMD command NULL
  Type 1 packet opcode NOP
  Type 1 packet opcode write register CMD command RCRC
  Type 1 packet opcode NOP (2 consecutive)
  Type 1 packet opcode write register FAR words 00000000
  Type 1 packet opcode write register RBCRC_SW words 00000000
  Type 1 packet opcode write register COR0 words 38083FE5
  Type 1 packet opcode write register COR1 words 00400000
  Type 1 packet opcode write register IDCODE KU3P
  Type 1 packet opcode write register CMD command FALL_EDGE
  Type 1 packet opcode write register CMD command SWITCH
  Type 1 packet opcode NOP
  Type 1 packet opcode write register MASK words 00000001
  Type 1 packet opcode write register CTL0 words 00000101
  Type 1 packet opcode write register MASK words 00001000
  Type 1 packet opcode write register CTL1 words 00001000
  Type 1 packet opcode NOP (8 consecutive)
  Configuration data writes consisting of:
    134708 NOPs
    11794 FAR writes
    318 WCFG commands
    318 FDRI writes with a total of 61752 words
    58 MFW commands
    58 NULL commands
    11476 MFWR writes with a total of 160664 words
    140 Type 2 packets with a total of 2756892 words
  Type 1 packet opcode write register CRC words EEFE3AE6
  Type 1 packet opcode NOP (2 consecutive)
  Type 1 packet opcode write register CMD command GRESTORE
  Type 1 packet opcode NOP (2 consecutive)
  Type 1 packet opcode write register CMD command DGHIGH_LFRM
  Type 1 packet opcode NOP (20 consecutive)
  Type 1 packet opcode write register MASK words 00001000
  Type 1 packet opcode write register CTL1 words 00000000
  Type 1 packet opcode write register CMD command START
  Type 1 packet opcode NOP
  Type 1 packet opcode write register FAR words 07FC0000
  Type 1 packet opcode write register MASK words 00000101
  Type 1 packet opcode write register CTL0 words 00000101
  Type 1 packet opcode write register CRC words 5FFE959E
  Type 1 packet opcode NOP (2 consecutive)
  Type 1 packet opcode write register CMD command DESYNC
  Type 1 packet opcode NOP (393 consecutive)
```

## 3. PCIe 设计首次尝试

本节使用 AS02MC04_dma_stream_crc64 设计。

## 3.1. HP Pavilion 590-p0053na 台式机

该设计多次尝试均未能成功在 PCIe 总线上枚举。

在 DMA bridge IP 中启用了 JTAG 调试器。未能获得枚举失败的明确原因。有时运行 `source AS02MC04_dma_stream_crc64/AS02MC04_dma_stream_crc64.gen/sources_1/bd/AS02MC04_dma_stream_crc64/ip/AS02MC04_dma_stream_crc64_xdma_0_0/ip_0/AS02MC04_dma_stream_crc64_xdma_0_0_pcie4_ip/pcie_debugger/test_rd.tcl` 会报告 AXI 超时（未记录确切错误）。

## 3.2. Intel DH67BL 主板

Intel DH67BL 主板配备 i5-2310 CPU，运行 AlmaLinux 8.10。

与在 HP Pavilion 590-p0053na 中未能枚举的相同 FPGA 设计，在该 PC 中同样未能枚举。

根据 [AS02MC04_dma_stream_crc64](https://gist.github.com/Chester-Gillon/cf17eb97c63e754de4e06bc6552ffdb7#22--as02mc04_dma_stream_crc64)，在该 PC 中 VFIO 打开后链路速度训练为 2.5 GT/s，而非该 PC 支持的预期 5 GT/s。这与在该 PC 中使用的其他 FPGA 板卡遇到的问题相同。

尚未见 `pcie_debugger/test_rd.tcl` 因超时而失败。

## 4. PCIe 枚举问题

描述使用基于 _DMA/Bridge Subsystem for PCI Express (4.2)_ 的 AS02MC04_dma_stream_crc64 设计，尝试使 PCIe endpoint 以 Gen3 速度 x8 宽度枚举时遇到的问题。

## 4.1. PCIe 通道约束（参考示例）

最初使用以下约束，与 [part0_pins.xml](https://github.com/TiferKing/as02mc04_hack/blob/main/as02mc04/1.0/part0_pins.xml) 匹配：
```tcl
set_property PACKAGE_PIN P2 [get_ports {pcie_7x_mgt_rxp[0]}]
set_property PACKAGE_PIN T2 [get_ports {pcie_7x_mgt_rxp[1]}]
set_property PACKAGE_PIN V2 [get_ports {pcie_7x_mgt_rxp[2]}]
set_property PACKAGE_PIN Y2 [get_ports {pcie_7x_mgt_rxp[3]}]
set_property PACKAGE_PIN AB2 [get_ports {pcie_7x_mgt_rxp[4]}]
set_property PACKAGE_PIN AD2 [get_ports {pcie_7x_mgt_rxp[5]}]
set_property PACKAGE_PIN AE4 [get_ports {pcie_7x_mgt_rxp[6]}]
set_property PACKAGE_PIN AF2 [get_ports {pcie_7x_mgt_rxp[7]}]
```

## 4.1.1. HP Pavilion 590-p0053na 台式机 x16 PCIe 插槽

未能枚举。

## 4.1.2. Intel DH67BL 主板

Intel DH67BL 主板配备 i5-2310 CPU。

以 x8 宽度枚举成功。速度为 Gen1，但可以重新训练至 Gen2，这是该 PC 支持的最快速度。

## 4.1.3. HP Z6 G4 插槽 4

以 Gen3 x4 宽度枚举成功。

当 SSD1 未安装时该插槽为 x8 宽度，BIOS 显示支持 bifurcation 为 x8 或 x4x4。

将 BIOS bifurcation 从 Auto 改为 x8 后，仍然以 x4 宽度枚举。

## 4.1.4. HP Z6 G4 插槽 2

未能枚举。

该插槽为 Gen3 x16。

## 4.1.5. HP Z4 G4 插槽 5

未能枚举。

该插槽为 Gen3 x8，BIOS 显示支持 bifurcation 为 x8 或 x4x4。

## 4.2. 约束中反转通道顺序

重新构建 AS02MC04_dma_stream_crc64，修改约束以在 FPGA 引脚处反转所有 8 个通道，与逆向工程示例相比。即使用：
```tcl
set_property PACKAGE_PIN P2 [get_ports {pcie_7x_mgt_rxp[7]}]
set_property PACKAGE_PIN T2 [get_ports {pcie_7x_mgt_rxp[6]}]
set_property PACKAGE_PIN V2 [get_ports {pcie_7x_mgt_rxp[5]}]
set_property PACKAGE_PIN Y2 [get_ports {pcie_7x_mgt_rxp[4]}]
set_property PACKAGE_PIN AB2 [get_ports {pcie_7x_mgt_rxp[3]}]
set_property PACKAGE_PIN AD2 [get_ports {pcie_7x_mgt_rxp[2]}]
set_property PACKAGE_PIN AE4 [get_ports {pcie_7x_mgt_rxp[1]}]
set_property PACKAGE_PIN AF2 [get_ports {pcie_7x_mgt_rxp[0]}]
```
将板卡安装到 HP Z4 G4 插槽中，在两个比特流之间反复切换的结果如下：
1. 使用原始通道映射，未能枚举
2. 使用反转通道映射，以 x4 宽度枚举成功。

使用反转通道映射枚举成功后：
```
[mr_halfword@skylake-alma release]$ identify_pcie_fpga_design/display_identified_pcie_fpga_designs -d 0000:2d:00.0
Opening device 0000:2d:00.0 (10ee:9038) with IOMMU group 25
Enabled bus master for 0000:2d:00.0
Warning: Device device 0000:2d:00.0 (10ee:9038) has reduced bandwidth
         Max width x8 speed 8 GT/s. Negotiated width x4 speed 8 GT/s

Design AS02MC04_dma_stream_crc64:
  PCI device 0000:2d:00.0 rev 00 IOMMU group 25  physical slot 5-2

  DMA bridge bar 2 AXI Stream
  Channel ID  addr_alignment  len_granularity  num_address_bits
       H2C 0               1                1                64
       H2C 1               1                1                64
       H2C 2               1                1                64
       H2C 3               1                1                64
       C2H 0               1                1                64
       C2H 1               1                1                64
       C2H 2               1                1                64
       C2H 3               1                1                64
  User access build timestamp : 14332B0F - 02/08/2025 18:44:15
  Quad SPI registers at bar 0 offset 0x0
  SYSMON registers at bar 0 offset 0x1000
```
_UltraScale+ Devices Integrated Block for PCI Express Product Guide (PG213)_ 中的 [Lane Reversal](https://docs.amd.com/r/en-US/pg213-pcie4-ultrascale-plus/Lane-Reversal) 说明了何时支持通道反转。

## 4.3. 不同 FPGA 配置调查 - 当配置存储器包含无法枚举的设计时

本节使用安装于 HP Z4 G4 插槽 5 的板卡。

BIOS PCIe 相关设置如下：
 - System Options -> PCIe Training Reset -> Enable（默认为 Disable，在之前的测试中已启用）
 - Slot Settings -> Slot 5 PCI Express x8
   - Option ROM Download -> Enable
   - Limit PCIe Speed -> Auto
   - Bifurcation -> Auto
   - Resizable Bars -> Disable

由于 [Windows 11 更新至 24H2 后无法启动](https://gist.github.com/Chester-Gillon/5b09a5c0609aded0ff93faaa14724158)，无法启动 Windows 运行将所有设置保存到文件的工具，因此上述设置是在 BIOS GUI 中检查的。

之前曾使用 Windows 工具在插槽 5 上启用热插拔支持，这是一个在 BIOS GUI 中不可见的设置。

## 4.3.1. 配置存储器中修改版 AS02MC04_dma_stream_crc64

FPGA 的配置存储器包含一个本地修改的 AS02MC04_dma_stream_crc64 版本，该版本不受 GIT 控制：
- 红色而非绿色的板边 LED 表示 PCIe 链路已建立。这样更容易观察，因为板卡上的其他 LED 都是绿色的。
- 约束中通道已反转

开机时 PCIe 链路未能建立。有两个根端口，物理插槽为 `5` 和 `5-1`：
```
$ dump_info/display_physical_slots 
Access method : linux-sysfs
domain=0000 bus=15 dev=00 func=00
  vendor_id=10ee (Xilinx Corporation) device_id=7024 (Device 7024)
  physical slot: 3-1
domain=0000 bus=2c dev=02 func=00
  vendor_id=8086 (Intel Corporation) device_id=2032 (Sky Lake-E PCI Express Root Port C)
  physical slot: 6
domain=0000 bus=00 dev=1d func=00
  vendor_id=8086 (Intel Corporation) device_id=a298 (200 Series PCH PCI Express Root Port #9)
  physical slot: 4
domain=0000 bus=2c dev=03 func=00
  vendor_id=8086 (Intel Corporation) device_id=2033 (Sky Lake-E PCI Express Root Port D)
  physical slot: 7
domain=0000 bus=2c dev=00 func=00
  vendor_id=8086 (Intel Corporation) device_id=2030 (Sky Lake-E PCI Express Root Port A)
  physical slot: 5
domain=0000 bus=00 dev=1b func=00
  vendor_id=8086 (Intel Corporation) device_id=a2eb (200 Series PCH PCI Express Root Port #21)
  physical slot: 8191
domain=0000 bus=2c dev=01 func=00
  vendor_id=8086 (Intel Corporation) device_id=2031 (Sky Lake-E PCI Express Root Port B)
  physical slot: 5-1
domain=0000 bus=37 dev=00 func=00
  vendor_id=10ee (Xilinx Corporation) device_id=7011 (7-Series FPGA Hard PCIe block (AXI/debug))
  physical slot: 6-1
domain=0000 bus=14 dev=00 func=00
  vendor_id=8086 (Intel Corporation) device_id=2030 (Sky Lake-E PCI Express Root Port A)
  physical slot: 3
domain=0000 bus=20 dev=00 func=00
  vendor_id=8086 (Intel Corporation) device_id=2030 (Sky Lake-E PCI Express Root Port A)
  physical slot: 1
```
插槽 `5` 和 `5-1` 的两个根端口均为 x4 宽度。

如果在 Linux 启动后从配置存储器加载 FPGA，链路会建立，物理插槽 `5-2` 显示为 x4：
```
$ identify_pcie_fpga_design/display_identified_pcie_fpga_designs 
Opening device 0000:15:00.0 (10ee:7024) with IOMMU group 41
Enabled bus master for 0000:15:00.0
Opening device 0000:2d:00.0 (10ee:9038) with IOMMU group 25
Enabled bus master for 0000:2d:00.0
Warning: Device device 0000:2d:00.0 (10ee:9038) has reduced bandwidth
         Max width x8 speed 8 GT/s. Negotiated width x4 speed 8 GT/s
Opening device 0000:37:00.0 (10ee:7011) with IOMMU group 86
Enabled bus master for 0000:37:00.0

Design dma_blkram (TEF1001):
  PCI device 0000:15:00.0 rev 00 IOMMU group 41  physical slot 3-1

  DMA bridge bar 0 memory size 0x120000
  Channel ID  addr_alignment  len_granularity  num_address_bits
       H2C 0               1                1                64
       H2C 1               1                1                64
       C2H 0               1                1                64
       C2H 1               1                1                64

Design AS02MC04_dma_stream_crc64:
  PCI device 0000:2d:00.0 rev 00 IOMMU group 25  physical slot 5-2

  DMA bridge bar 2 AXI Stream
  Channel ID  addr_alignment  len_granularity  num_address_bits
       H2C 0               1                1                64
       H2C 1               1                1                64
       H2C 2               1                1                64
       H2C 3               1                1                64
       C2H 0               1                1                64
       C2H 1               1                1                64
       C2H 2               1                1                64
       C2H 3               1                1                64
  User access build timestamp : 14332B0F - 02/08/2025 18:44:15
  Quad SPI registers at bar 0 offset 0x0
  SYSMON registers at bar 0 offset 0x1000

Design Nitefury Project-0 version 0x2:
  PCI device 0000:37:00.0 rev 00 IOMMU group 86  physical slot 6-1

  DMA bridge bar 2 memory size 0x40000000
  Channel ID  addr_alignment  len_granularity  num_address_bits
       H2C 0               1                1                64
       C2H 0               1                1                64
  Quad SPI registers at bar 0 offset 0x10000
  XADC registers at bar 0 offset 0x3000
```
测试结果：
```
$ xilinx_dma_bridge_for_pcie/crc64_stream_latency -t
Opening device 0000:15:00.0 (10ee:7024) with IOMMU group 41
Enabled bus master for 0000:15:00.0
Opening device 0000:2d:00.0 (10ee:9038) with IOMMU group 25
Enabled bus master for 0000:2d:00.0
Warning: Device device 0000:2d:00.0 (10ee:9038) has reduced bandwidth
         Max width x8 speed 8 GT/s. Negotiated width x4 speed 8 GT/s
Opening device 0000:37:00.0 (10ee:7011) with IOMMU group 86
Enabled bus master for 0000:37:00.0
Testing design AS02MC04_dma_stream_crc64 using C2H 0 -> H2C 0
Current temperature  41.3C  min  39.3C  max  44.3C
     32 len bytes latencies (us):   2.642 (50')   2.780 (75')   3.258 (99')  18.868 (99.999')
     64 len bytes latencies (us):   2.619 (50')   2.624 (75')   2.645 (99')  13.076 (99.999')
    128 len bytes latencies (us):   2.653 (50')   2.662 (75')   2.683 (99')  18.028 (99.999')
    256 len bytes latencies (us):   2.724 (50')   2.733 (75')   2.754 (99')  11.898 (99.999')
    512 len bytes latencies (us):   2.832 (50')   2.840 (75')   2.865 (99')  13.051 (99.999')
   1024 len bytes latencies (us):   2.974 (50')   2.981 (75')   3.008 (99')  18.005 (99.999')
   2048 len bytes latencies (us):   3.263 (50')   3.276 (75')   3.310 (99')  17.748 (99.999')
   4096 len bytes latencies (us):   3.861 (50')   3.872 (75')   3.905 (99')  19.126 (99.999')
   8192 len bytes latencies (us):   4.986 (50')   5.000 (75')   5.034 (99')  20.762 (99.999')
  16384 len bytes latencies (us):   7.297 (50')   7.310 (75')   7.339 (99')  23.028 (99.999')
  32768 len bytes latencies (us):  12.014 (50')  12.043 (75')  12.387 (99')  25.745 (99.999')
  65536 len bytes latencies (us):  21.370 (50')  21.419 (75')  21.729 (99')  38.491 (99.999')
 131072 len bytes latencies (us):  40.062 (50')  40.090 (75')  40.548 (99')  55.257 (99.999')
 262144 len bytes latencies (us):  77.365 (50')  77.421 (75')  77.838 (99')  92.264 (99.999')
 524288 len bytes latencies (us): 152.354 (50') 152.530 (75') 152.675 (99') 166.228 (99.999')
1048576 len bytes latencies (us): 301.333 (50') 301.380 (75') 301.905 (99') 315.720 (99.999')
Current temperature  41.3C  min  39.3C  max  45.3C
```
重启时链路断开且无法恢复。

## 4.3.2 gen3_x8_example_lane_order

未能枚举，无论是通过 JTAG 加载还是后续重启。

## 4.3.3. gen3_x8_reversed_lane_order

通过 JTAG 加载后可枚举，物理插槽 5-2 显示为 x4 宽度。

如果加载设计后重启，链路断开且重启后无法恢复。

从 v2025.1 降级至 v2024.2 以在 XDMA 中启用 JTAG 调试器并避免 [2.3. Investigating JTAG debugger](https://gist.github.com/Chester-Gillon/0cd96b77ae400ba2eb45745e640f1f5f#23-investigating-jtag-debugger) 中 JTAG 调试器无法工作的问题后，物理插槽 5-2 仍以 x4 宽度枚举：
```
$ identify_pcie_fpga_design/display_identified_pcie_fpga_designs 
Opening device 0000:2d:00.0 (10ee:9038) with IOMMU group 26
Enabled bus master for 0000:2d:00.0
Warning: Device device 0000:2d:00.0 (10ee:9038) has reduced bandwidth
         Max width x8 speed 8 GT/s. Negotiated width x4 speed 8 GT/s
Opening device 0000:37:00.0 (10ee:7011) with IOMMU group 86
Enabled bus master for 0000:37:00.0

Design AS02MC04_enum:
  PCI device 0000:2d:00.0 rev 01 IOMMU group 26  physical slot 5-2

  DMA bridge bar 1 memory size 0x1000
  Channel ID  addr_alignment  len_granularity  num_address_bits
       H2C 0               1                1                64
       C2H 0               1                1                64
  User access build timestamp : FC334B0A - 31/08/2025 20:44:10

Design Nitefury Project-0 version 0x2:
  PCI device 0000:37:00.0 rev 00 IOMMU group 86  physical slot: 6-1

  DMA bridge bar 2 memory size 0x40000000
  Channel ID  addr_alignment  len_granularity  num_address_bits
       H2C 0               1                1                64
       C2H 0               1                1                64
  Quad SPI registers at bar 0 offset 0x10000
  XADC registers at bar 0 offset 0x3000
```
`test_rd.tcl` 脚本运行结果：
```
source -notrace AS02MC04_enum/AS02MC04_enum.gen/sources_1/bd/AS02MC04_enum/ip/AS02MC04_enum_xdma_0_0/ip_0/AS02MC04_enum_xdma_0_0_pcie4_ip/pcie_debugger/test_rd.tcl
Identified PCIe debugger with hw_axi_1
INFO: [Labtoolstcl 44-481] READ DATA is: 00000901
INFO: [Labtoolstcl 44-481] READ DATA is: 00000001
INFO: [Labtoolstcl 44-481] READ DATA is: 00000009
INFO: [Labtoolstcl 44-481] READ DATA is: 00000000
INFO: [Labtoolstcl 44-481] READ DATA is: 00000000
INFO: [Labtoolstcl 44-481] READ DATA is: 00001001
INFO: [Labtoolstcl 44-481] READ DATA is: 0000abcd
INFO: [Labtoolstcl 44-481] READ DATA is: 00000000
phy_lane : 8
width : 04
speed : 03
```
`draw_rxdet.tcl` 显示所有 8 个通道均检测到接收器：
```
$ wish AS02MC04_enum/AS02MC04_enum.gen/sources_1/bd/AS02MC04_enum/ip/AS02MC04_enum_xdma_0_0/ip_0/AS02MC04_enum_xdma_0_0_pcie4_ip/pcie_debugger/draw_rxdet.tcl 
phy_lane : 8
width :  4
speed : 03
count = 1
 i = 0
count = 2
 i = 0
line1 = 13
Receiver detected in lane 0
count = 3
 i = 0
Receiver detected in lane 0
count = 4
 i = 0
Receiver detected in lane 0
count = 5
 i = 4
count = 6
 i = 4
line1 = 13
Receiver detected in lane 1
count = 7
 i = 4
Receiver detected in lane 1
count = 8
 i = 4
Receiver detected in lane 1
count = 9
 i = 8
count = 10
 i = 8
line1 = 13
Receiver detected in lane 2
count = 11
 i = 8
Receiver detected in lane 2
count = 12
 i = 8
Receiver detected in lane 2
count = 13
 i = 12
count = 14
 i = 12
line1 = 13
Receiver detected in lane 3
count = 15
 i = 12
Receiver detected in lane 3
count = 16
 i = 12
Receiver detected in lane 3
count = 17
 i = 16
count = 18
 i = 16
```

## 5. 总结与建议

### 5.1. PCIe 通道映射问题

根据测试结果，AS02MC04 板卡的 PCIe 通道映射与标准逆向工程示例存在差异。使用反转通道映射可以成功枚举，但仅能达到 x4 宽度而非预期的 x8。

### 5.2. 可能原因分析

1. **板卡硬件差异**：不同批次板卡可能使用不同速度等级的器件（项目中指定 2E，实际 DNA 读取为 1E）
2. **通道顺序反转**：需要在约束文件中反转通道顺序才能在部分系统中成功枚举
3. **链路训练问题**：即使使用正确的通道映射，也只能达到 x4 宽度，可能是硬件或 BIOS 配置问题

### 5.3. 建议解决方案

1. 验证 BIOS 中 PCIe bifurcation 设置是否正确
2. 确认目标系统的 PCIe 插槽是否支持 x8 宽度的 Gen3 速度
3. 如需要 x8 宽度，考虑联系板卡供应商确认通道映射规范
4. 使用 JTAG 调试器进一步分析链路训练过程中的具体问题

---
**文档翻译说明**：本中文版本由英文原版翻译，保留了所有技术细节、命令输出和代码片段。如需参考原始英文版本，请查看 `AS02MC04_card.md`。