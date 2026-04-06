# SOCS HLS Makefile
# FPGA-Litho Project

# 项目根目录
PROJ_ROOT = /root/project/FPGA-Litho/source/SOCS_HLS

# HLS工作目录
HLS_WORK_DIR = hls/socs_proj

# 配置文件
HLS_CONFIG = hls/script/config/hls_config.cfg

# vitis-run 命令（严格按copilot-instructions）
VITIS_RUN = vitis-run

# ============================================================================
# 主要目标
# ============================================================================

all: help

help:
	@echo "SOCS HLS Build Targets:"
	@echo "  make csim      - C仿真"
	@echo "  make csynth    - C综合"
	@echo "  make cosim     - CoSim验证"
	@echo "  make package   - IP导出"
	@echo "  make clean     - 清理工作目录"
	@echo "  make golden    - 生成Golden数据"

# ============================================================================
# Golden数据生成
# ============================================================================

golden:
	@echo "[GOLDEN] Generating test data..."
	python3 generate_golden.py
	@echo "[GOLDEN] Done!"

# ============================================================================
# HLS仿真流程（使用 vitis-run）
# ============================================================================

csim:
	@echo "[CSIM] Running C Simulation..."
	cd $(PROJ_ROOT)
	$(VITIS_RUN) --mode hls --csim --config $(HLS_CONFIG) --work_dir $(HLS_WORK_DIR)
	@echo "[CSIM] Done!"

csynth:
	@echo "[CSYNTH] Running C Synthesis..."
	cd $(PROJ_ROOT)
	$(VITIS_RUN) --mode hls --tcl --input_file hls/script/run_csynth.tcl --work_dir $(HLS_WORK_DIR)
	@echo "[CSYNTH] Done!"

cosim:
	@echo "[COSIM] Running Co-Simulation..."
	cd $(PROJ_ROOT)
	$(VITIS_RUN) --mode hls --cosim --config $(HLS_CONFIG) --work_dir $(HLS_WORK_DIR)
	@echo "[COSIM] Done!"

package:
	@echo "[PACKAGE] Exporting Vivado IP..."
	cd $(PROJ_ROOT)
	$(VITIS_RUN) --mode hls --package --config $(HLS_CONFIG) --work_dir $(HLS_WORK_DIR)
	@echo "[PACKAGE] IP exported to $(HLS_WORK_DIR)/solution1/impl/export/ip/"

# ============================================================================
# 清理
# ============================================================================

clean:
	@echo "[CLEAN] Removing work directory..."
	rm -rf $(HLS_WORK_DIR)
	@echo "[CLEAN] Done!"

# ============================================================================
# 辅助目标
# ============================================================================

check-tools:
	@echo "[CHECK] Verifying vitis-run availability..."
	which $(VITIS_RUN) || (echo "ERROR: vitis-run not found!" && exit 1)
	@echo "[CHECK] Tools OK!"

.PHONY: all help golden csim csynth cosim package clean check-tools