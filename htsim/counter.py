def analyze_flows(filename):
    all_fcts = []        # 所有流的FCT
    small_fcts = []      # <100KB流的FCT
    large_fcts = []      # >=100KB流的FCT
    
    with open(filename, 'r') as f:
        for line in f:
            # 檢查是否是Flow記錄行
            if line.startswith('Flow'):
                # 解析Flow信息
                parts = line.split()
                
                # 獲取flow size和FCT
                size_index = parts.index('size')
                fct_index = parts.index('fct')
                
                size = int(parts[size_index + 1])
                fct = float(parts[fct_index + 1])
                
                # 添加到相應的列表
                all_fcts.append(fct)
                
                if size < 100000:  # <100KB
                    small_fcts.append(fct)
                else:              # >=100KB
                    large_fcts.append(fct)
    
    # 計算平均值
    avg_all = sum(all_fcts) / len(all_fcts) if all_fcts else 0
    avg_small = sum(small_fcts) / len(small_fcts) if small_fcts else 0
    avg_large = sum(large_fcts) / len(large_fcts) if large_fcts else 0
    
    # 輸出結果
    print("-" * 70)
    print(f"All flows average FCT: {avg_all:.2f} ms (total flows: {len(all_fcts)})")
    print(f"Small flows (<100KB) average FCT: {avg_small:.2f} ms (total flows: {len(small_fcts)})")
    print(f"Large flows (>=100KB) average FCT: {avg_large:.2f} ms (total flows: {len(large_fcts)})")
    print("-" * 70)
    
    return avg_all, avg_small, avg_large

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python script.py <log_file>")
        sys.exit(1)
    
    filename = sys.argv[1]
    analyze_flows(filename)