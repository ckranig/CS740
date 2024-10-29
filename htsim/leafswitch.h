// leafswitch.h - 定義leaf switch的資料結構和方法
#ifndef LEAF_SWITCH_H 
#define LEAF_SWITCH_H

#include "network.h"
#include "queue.h"
#include <unordered_map>
#include <vector>

// Congestion level計算相關常數
#define CONGA_FLOWLET_TIMEOUT 100 // 100us timeout
#define CONGA_QUANTIZATION_BITS 3 // 3-bit量化
#define CONGA_MAX_CONGESTION ((1 << CONGA_QUANTIZATION_BITS) - 1)

#define NUMOFCORE 12

class LeafSwitch {
public:
    LeafSwitch(int id);
    
    void updateCongestionToLeaf(int leafId, int congestionLevel);
    void updateCongestionFromLeaf(int leafId, int congestionLevel);
    int selectBestUplink(int destLeafId);
    int measureLocalCongestion(Queue* queue);
    void initializeCongestionToLeaf(int coreId);

    long long uplink[12];
    long long downlink[12];
    long long congestionToLeaf[12][24];
    long long congestionFromLeaf[12][24];
    simtime_picosec timeStamp;
    int id;

private:
    // Leaf switch ID
    // Congestion tables
    //std::unordered_map<int, std::vector<int>> _congestionToLeaf;   // 到目標leaf的擁塞程度
    //std::unordered_map<int, int> _congestionFromLeaf;  // 來自source leaf的擁塞程度
    
   

    


    // Helper methods
    //int quantizeCongestion(double rawCongestion);
};

#endif
