/**
 * @file kernel_extractor.cpp
 * @brief SOCS 本征核提取模块 —— TCC 矩阵特征值分解
 * 
 * 本文件实现了 SOCS 算法中的关键步骤：从 TCC 矩阵中提取本征核和特征值。
 * 
 * 数学原理：
 * TCC 矩阵为 Hermitian 矩阵（TCC[i,j] = conj(TCC[j,i])），
 * 通过特征值分解提取前 nk 个最大特征值及其对应的特征向量：
 * \f[
 * \text{TCC} = \sum_{k=1}^{nk} \sigma_k K_k K_k^\dagger
 * \f]
 * 
 * 其中 \f$\sigma_k\f$ 为特征值（权重），\f$K_k\f$ 为本征核（特征向量）。
 * 
 * @author FPGA-Litho Project
 * @version 1.0
 */

/**
 * @brief 从 TCC 矩阵提取 SOCS 本征核和特征值
 * 
 * 该函数对 Hermitian TCC 矩阵进行特征值分解，提取前 nk 个最大特征值
 * 及其对应的特征向量（本征核）。
 * 
 * 实现步骤：
 * 1. 将 TCC 复制到 Eigen 矩阵（列主序，与 std::vector 兼容）
 * 2. 使用 SelfAdjointEigenSolver 进行 Hermitian 特征值分解
 * 3. 提取前 nk 个最大特征值和特征向量
 * 4. 反转顺序（Eigen 返回从小到大，需从大到小）
 * 5. 格式化输出核和特征值
 * 
 * @param[out] krns     本征核数组（nk 个核，每个核大小为 tccSize）
 * @param[out] scales   特征值数组（nk 个特征值）
 * @param[in] nk        提取的核数量
 * @param[in] tcc       TCC 矩阵（Hermitian，大小为 tccSize × tccSize）
 * @param[in] tccSize   TCC 矩阵尺寸
 * @param[in] Nx        频域采样点数（x 方向）
 * @param[in] Ny        频域采样点数（y 方向）
 * @param[in] verbose   详细输出级别
 * 
 * @return 0 表示成功，负数表示错误
 * 
 * @note TCC 矩阵为 Hermitian 矩阵，特征值为实数，特征向量正交归一化。
 */
int extractKernels(std::vector<std::vector<ComplexD>>& krns,
                   std::vector<float>& scales, int nk,
                   const std::vector<ComplexD>& tcc,
                   int tccSize, int Nx, int Ny, int verbose) {
    
    // 输入验证
    if (tccSize * tccSize != static_cast<int>(tcc.size())) {
        std::cerr << "[extractKernels] 错误：TCC 尺寸不匹配！" << std::endl;
        return -1;
    }
    
    if (nk > tccSize) {
        std::cerr << "[extractKernels] 错误：nk > tccSize！" << std::endl;
        return -2;
    }
    
    if (verbose >= 1) {
        std::cout << "[extractKernels] 正在求解 TCC 矩阵的特征值问题..." << std::endl;
        std::cout << "  TCC 尺寸: " << tccSize << " × " << tccSize << std::endl;
        std::cout << "  请求核数量: " << nk << std::endl;
    }
    
    // 步骤 1：将 TCC 复制到 Eigen 矩阵
    // Eigen 使用列主序存储，与我们的行主序 TCC 布局兼容
    // 需要在复制时转置以确保正确的矩阵表示
    MatrixXcd eigenTcc(tccSize, tccSize);
    
    for (int i = 0; i < tccSize; i++) {
        for (int j = 0; j < tccSize; j++) {
            // 原始 TCC: tcc[i * tccSize + j]（行主序）
            // Eigen 矩阵: eigenTcc(i, j)（列主序，但直接索引有效）
            eigenTcc(i, j) = tcc[j * tccSize + i];  // 转置以获得正确方向
        }
    }
    
    // 步骤 2：求解特征值问题
    // SelfAdjointEigenSolver 假设矩阵为 Hermitian
    // 返回的特征值按从小到大排序
    Eigen::SelfAdjointEigenSolver<MatrixXcd> solver(eigenTcc);
    
    if (solver.info() != Eigen::Success) {
        std::cerr << "[extractKernels] 错误：特征值分解失败！" << std::endl;
        return -3;
    }
    
    // 步骤 3：提取特征值（按从小到大排序）
    VectorXd eigenvalues = solver.eigenvalues();
    
    // 步骤 4：提取特征向量（特征向量矩阵的列）
    // 特征向量按对应特征值排序（最小在前）
    MatrixXcd eigenvectors = solver.eigenvectors();
    
    if (verbose >= 1) {
        std::cout << "[extractKernels] 特征值分解成功。" << std::endl;
        std::cout << "  前 " << nk << " 个特征值（最大）:" << std::endl;
        for (int i = tccSize - nk; i < tccSize; i++) {
            std::cout << "    lambda[" << i << "] = " << eigenvalues[i] << std::endl;
        }
    }
    
    // 步骤 5：提取前 nk 个特征值和特征向量（反转顺序）
    // Eigen 返回从小到大，我们需要从大到小
    scales.resize(nk);
    krns.resize(nk);
    for (int k = 0; k < nk; k++) {
        krns[k].resize(tccSize);
    }
    
    for (int j = 0; j < nk; j++) {
        // 特征值索引（反转顺序：最大在前）
        int eigenIdx = tccSize - 1 - j;
        
        // 特征值（权重）
        scales[j] = static_cast<float>(eigenvalues[eigenIdx]);
        
        // 本征核（特征向量，向量内反转顺序）
        // 匹配 litho.cpp 行为：krns[i][j] = z[(m-1-i)*n + (n-1-j)]
        for (int i = 0; i < tccSize; i++) {
            krns[j][i] = eigenvectors.col(eigenIdx)(tccSize - 1 - i);
        }
    }
    
    if (verbose >= 1) {
        std::cout << "[extractKernels] 本征核提取成功。" << std::endl;
        std::cout << "  总核能量：sum(scales) = ";
        double total = 0.0;
        for (int k = 0; k < nk; k++) {
            total += scales[k];
        }
        std::cout << total << std::endl;
    }
    
    return 0;  // 成功
}

/**
 * @brief 将本征核重构为二维空间频率格式
 * 
 * 该函数将一维特征向量重新排列为二维格式：
 *   Kernel[k][y × (2×Nx+1) + x]，其中 y = -Ny 到 Ny，x = -Nx 到 Nx
 * 
 * @param[out] krns2D   二维格式本征核数组
 * @param[in] krns      一维格式本征核数组
 * @param[in] nk        核数量
 * @param[in] tccSize   TCC 矩阵尺寸
 * @param[in] Nx        频域采样点数（x 方向）
 * @param[in] Ny        频域采样点数（y 方向）
 * 
 * @note 提取过程已隐式完成二维排列，此函数仅用于显式二维访问。
 */
void reconstructKernels2D(std::vector<std::vector<ComplexD>>& krns2D,
                          const std::vector<std::vector<ComplexD>>& krns,
                          int nk, int tccSize, int Nx, int Ny) {
    
    int kerX = 2 * Nx + 1;  ///< 本征核 x 方向尺寸
    int kerY = 2 * Ny + 1;  ///< 本征核 y 方向尺寸
    
    krns2D.resize(nk);
    for (int k = 0; k < nk; k++) {
        krns2D[k].resize(kerX * kerY);
    }
    
    // 一维核已具有正确顺序：
    // Index = (y + Ny) × (2×Nx+1) + (x + Nx)
    // 因此只需复制（无需重排）
    for (int k = 0; k < nk; k++) {
        krns2D[k] = krns[k];
    }
}