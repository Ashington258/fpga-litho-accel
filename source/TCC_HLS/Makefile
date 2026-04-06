# ============================================================================
# TCC HLS重构项目 - Makefile
# ============================================================================
# 参考：reference/参考文档/FPGA-Litho HLS-to-FPGA Workflow & TCL Verification Guide.md
# 器件：xcku3p-ffvb676-2-e
# 时钟：5ns (200 MHz)
# ============================================================================

.PHONY: all clean csim csynth cosim package verify help

# ============================================================================
# 项目配置
# ============================================================================
HLS_CONFIG = hls/script/config/hls_config.cfg
HLS_WORK_DIR = hls_litho_proj
HLS_TOP = tcc_top

# ============================================================================
# 默认目标：全流程（Phase 1-4完成后启用）
# ============================================================================
all: csim csynth cosim package

# ============================================================================
# Phase 0: 验证骨架
# ============================================================================
verify:
	@echo "========================================"
	@echo "Phase 0 Verification"
	@echo "========================================"
	@echo ""
	@echo "1. Checking directory structure..."
	@ls -R hls/ | head -20
	@echo ""
	@echo "2. Verifying data_types.h compilation..."
	@g++ -std=c++11 -c hls/src/data_types.h -o /dev/null && echo "✓ data_types.h compiles" || echo "✗ data_types.h compilation failed"
	@echo ""
	@echo "3. Verifying testbench compilation..."
	@g++ -std=c++11 -I hls/src hls/tb/tb_tcc.cpp -o tb_tcc && echo "✓ tb_tcc.cpp compiles" || echo "✗ tb_tcc.cpp compilation failed"
	@echo ""
	@echo "4. Checking golden data..."
	@ls -lh hls/tb/golden/ && echo "✓ Golden data exists" || echo "✗ Golden data missing"
	@echo ""
	@echo "5. Running testbench skeleton..."
	@./tb_tcc && echo "✓ Testbench runs successfully" || echo "✗ Testbench execution failed"
	@echo ""
	@echo "========================================"
	@echo "Phase 0 Verification Complete"
	@echo "========================================"

# ============================================================================
# Phase 1-4: HLS构建流程
# ============================================================================

# C仿真：验证算法正确性（对比golden数据）
csim:
	@echo "========================================"
	@echo "Running C Simulation"
	@echo "========================================"
	@echo "Command: vitis-run --mode hls --csim"
	vitis-run --mode hls --csim \
		--config $(HLS_CONFIG) \
		--work_dir $(HLS_WORK_DIR)
	@echo ""
	@echo "Check results in $(HLS_WORK_DIR)/solution1/csim/report/"

# C综合：生成RTL代码
csynth:
	@echo "========================================"
	@echo "Running C Synthesis"
	@echo "========================================"
	@echo "Command: vitis-run --mode hls --tcl"
	vitis-run --mode hls --tcl \
		--input_file hls/script/run_csynth.tcl \
		--work_dir $(HLS_WORK_DIR)
	@echo ""
	@echo "Check results in $(HLS_WORK_DIR)/solution1/syn/report/"

# CoSim：RTL级协同仿真
cosim:
	@echo "========================================"
	@echo "Running Co-Simulation"
	@echo "========================================"
	@echo "Command: vitis-run --mode hls --cosim"
	vitis-run --mode hls --cosim \
		--config $(HLS_CONFIG) \
		--work_dir $(HLS_WORK_DIR)
	@echo ""
	@echo "Check results in $(HLS_WORK_DIR)/solution1/cosim/report/"

# IP导出：生成Vivado IP核
package:
	@echo "========================================"
	@echo "Exporting Vivado IP"
	@echo "========================================"
	@echo "Command: vitis-run --mode hls --package"
	vitis-run --mode hls --package \
		--config $(HLS_CONFIG) \
		--work_dir $(HLS_WORK_DIR)
	@echo ""
	@echo "Generated IP in $(HLS_WORK_DIR)/solution1/impl/export/ip/"
	@ls -lh $(HLS_WORK_DIR)/solution1/impl/export/ip/*.zip

# ============================================================================
# 辅助目标
# ============================================================================

# 清理工作目录
clean:
	@echo "Cleaning HLS work directory..."
	rm -rf $(HLS_WORK_DIR)
	rm -f tb_tcc
	@echo "✓ Clean complete"

# 显示帮助
help:
	@echo "========================================"
	@echo "TCC HLS Build System"
	@echo "========================================"
	@echo ""
	@echo "Usage:"
	@echo "  make verify      - Phase 0 verification"
	@echo "  make csim        - C simulation"
	@echo "  make csynth      - C synthesis"
	@echo "  make cosim       - Co-simulation"
	@echo "  make package     - Export Vivado IP"
	@echo "  make all         - Full workflow (csim+csynth+cosim+package)"
	@echo "  make clean       - Clean work directory"
	@echo "  make help        - Show this help"
	@echo ""
	@echo "Current Phase: 0 (Skeleton Verification)"
	@echo "Phase 1-4 targets will be enabled after implementation"
	@echo ""
	@echo "Project structure:"
	@echo "  - Source: hls/src/"
	@echo "  - Testbench: hls/tb/"
	@echo "  - Config: hls/script/config/hls_config.cfg"
	@echo "  - Work dir: $(HLS_WORK_DIR)"
	@echo ""
	@echo "Device: xcku3p-ffvb676-2-e"
	@echo "Clock: 5ns (200 MHz)"

# ============================================================================
# Phase 1-4: 模块测试（后续启用）
# ============================================================================

# Phase 1: calcTCC模块测试
test_calc_tcc:
	@echo "Phase 1: Testing calc_tcc module..."
	g++ -std=c++11 -I hls/src hls/src/calc_tcc.cpp hls/tb/tb_tcc.cpp -o test_tcc
	./test_tcc

# Phase 2: FFT模块测试
test_fft_2d:
	@echo "Phase 2: Testing fft_2d module..."
	g++ -std=c++11 -I hls/src hls/src/fft_2d.cpp hls/tb/tb_tcc.cpp -o test_fft
	./test_fft

# Phase 3: calcImage模块测试
test_calc_image:
	@echo "Phase 3: Testing calc_image module..."
	g++ -std=c++11 -I hls/src hls/src/calc_image.cpp hls/tb/tb_tcc.cpp -o test_image
	./test_image

# ============================================================================
# Phase 5: Vivado验证（后期启用）
# ============================================================================

vivado_bd:
	@echo "Phase 5: Generating Vivado Block Design..."
	vivado -mode batch -source vivado_bd/design.tcl

board_test:
	@echo "Phase 5: Running board-level test..."
	vivado -mode batch -source vivado_bd/verify_bram.tcl