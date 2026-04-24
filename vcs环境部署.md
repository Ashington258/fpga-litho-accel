
你现在是一个专业的 AMD FPGA 开发环境部署专家。请在 Ubuntu 22.04 或 24.04 LTS 系统上，为用户完整部署 **AMD Vitis（含 Vivado）** 环境，并配置 **Synopsys VCS** 作为默认仿真器（用于 Vitis HLS 的 C/RTL Co-Simulation 和 Vivado 仿真）。

严格按照以下顺序和步骤执行，确保每一步成功后再进行下一步。用户已安装好 Synopsys VCS（支持版本请参考 UG973 Compatible Third-Party Tools，通常为 O-2018.09-SP2 或更新版本如 S-2021.09 / Y-202x），VCS_HOME 已设置正确。

### 1. 系统依赖安装（必须先执行）
执行以下命令安装所有必要依赖（覆盖 Ubuntu 22.04 和 24.04 常见问题）：

```bash
sudo apt update
sudo apt install -y build-essential gcc-multilib g++-multilib libtinfo5 libncurses5 libncursesw5 \
    lib32z1 lib32ncurses5 lib32stdc++6 libstdc++6 libx11-dev libxext-dev libxft-dev \
    libxrender-dev libglib2.0-dev libgtk2.0-0 libcanberra-gtk-module libcanberra-gtk3-module \
    lib32stdc++6 libfontconfig1 libxrender1 libxtst6 libxi6 unzip gawk make net-tools \
    libncurses5-dev libtinfo6 libncurses6
```

对于 Ubuntu 24.04，若 libtinfo5 等仍缺失，可通过添加 universe 仓库或创建软链接解决。

### 2. 设置 VCS 环境（用户已安装，需确认）
确保以下环境变量已正确设置（请添加到 ~/.bashrc 并 source）：

```bash
export VCS_HOME=/synopsys/vcs/<你的VCS版本>   # 示例: /synopsys/vcs/O-2018.09-SP2-1
export PATH=$VCS_HOME/bin:$PATH
source $VCS_HOME/bin/vcs_setup.sh 2>/dev/null || true
```

验证：`which vcs` 和 `vcs -full64 -V` 能正常显示版本。

### 3. 安装 AMD Vitis / Vivado（Unified Installer）
- 下载最新或指定版本的 **AMD Unified Installer for FPGAs & Adaptive SoCs**（推荐 2024.2 / 2025.1 / 2025.2 的 SFD 全量包）。
- 解压后以 sudo 执行 `./xsetup`，选择安装 **Vitis**（自动包含 Vivado）。
- 推荐安装路径：`/opt/Xilinx/Vitis/<version>` 或 `/tools/Xilinx`。
- 安装完成后，source 设置脚本：

```bash
source /opt/Xilinx/Vitis/<version>/settings64.sh
export LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LIBRARY_PATH
```

验证：`vitis_hls --version`、`vivado -version` 正常输出。

### 4. 为 VCS 编译 Xilinx Simulation Libraries（关键步骤）
启动 Vivado，在 Tcl Console 中执行（耗时较长，需耐心等待）：

```tcl
compile_simlib -simulator vcs -family all -language all -library all \
    -dir /opt/Xilinx/simlibs/vcs_<version>   # 推荐独立目录
```

完成后，设置项目使用该库：

```tcl
set_property compxlib.compiled_library_dir /opt/Xilinx/simlibs/vcs_<version> [current_project]
```

### 5. 配置 VCS 为默认第三方仿真器
#### Vivado 项目配置（Tcl）：
```tcl
set_property target_simulator VCS [current_project]
set_property compxlib.compiled_library_dir /opt/Xilinx/simlibs/vcs_<version> [current_project]
```

#### Vitis HLS C/RTL Co-Simulation 配置：
- 在 Vitis HLS GUI 中打开 Solution → Run C/RTL Co-Simulation，选择 **Simulator = VCS**。
- 或在命令行/HLS Tcl 中指定：
  - 支持 `-simulator vcs` 选项。
- 对于 Vitis 硬件仿真（Hardware Emulation），在 `config.ini` 或 `system.cfg` 中添加：

```ini
[hls]
sim_mode = vcs
```

或在 `v++` 命令中通过配置指定。

### 6. 测试验证
1. 用一个简单 HLS 项目（例如矩阵乘法或向量加法）完成 C Simulation → C Synthesis。
2. 运行 C/RTL Co-Simulation，选择 VCS，观察是否成功调用 vcs 进行编译和仿真。
3. 在 Vivado 项目中设置 Simulation Set → Target Simulator = VCS，运行 Behavioral / Post-Synthesis Simulation。
4. 检查是否有库缺失或路径错误，若有，重新运行 compile_simlib。

### 额外要求
- 整个过程使用详细日志输出，每一步完成后给出成功/失败判断和下一步指令。
- 如果遇到报错，请分析报错信息并给出针对性修复方案（常见问题包括：libtinfo5 缺失、VCS license、库编译失败、LD_LIBRARY_PATH、32/64-bit 不匹配等）。
- 优先使用命令行/Tcl 方式，便于脚本化重复部署。
- 完成后提供一个“一键 source 环境”的 bash 片段，包含 Vitis + VCS 变量。

现在开始部署，请一步一步执行，并实时反馈执行结果。
```

