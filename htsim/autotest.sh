#!/bin/bash

# 清空或建立 stat.txt 檔案
> stat.txt

# 設定初始 utilization 值及增量
start=0.1
step=0.1
end=1.0

# 定義 flowdist 值列表
flowdists=("uniform" "pareto" "enterprise" "dataming")

# 迴圈執行不同的 utilization 值
utilization=$start
while (( $(echo "$utilization <= $end" | bc -l) )); do
    # 針對每一個 flowdist 值進行測試
    for flowdist in "${flowdists[@]}"; do
        # Conga 模式
        ./htsim --expt=2 --mode=0 --utilization=$utilization --flowdist=$flowdist > out.txt
        echo "Conga: --utilization=$utilization --flowdist=$flowdist" >> stat.txt
        python3 counter.py out.txt >> stat.txt

        # ECMP 模式
        ./htsim --expt=2 --mode=1 --utilization=$utilization --flowdist=$flowdist > out.txt
        echo "ECMP: --utilization=$utilization --flowdist=$flowdist" >> stat.txt
        python3 counter.py out.txt >> stat.txt
    done
    
    # 更新 utilization 值
    utilization=$(echo "$utilization + $step" | bc)
done

echo "所有測試完成！結果已寫入 stat.txt。"
