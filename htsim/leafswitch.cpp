#include "leafswitch.h"

//建構式
LeafSwitch::LeafSwitch(int id) : id(id) {
    //initial
    for(int i = 0; i < 12; i++) {
        uplink[i] = 0;
        downlink[i] = 0;
        for(int j = 0; j < 24; j++) {
            congestionToLeaf[i][j] = 0;
            congestionFromLeaf[i][j] = 0;
        }
    }
    timeStamp = 0;
}

void LeafSwitch::updateCongestionToLeaf(int leafId, int congestionLevel) {
    // _congestionToLeaf[leafId].push_back(congestionLevel);
    // // 只保留最近的N個測量值
    // if(_congestionToLeaf[leafId].size() > 10) {
    //     _congestionToLeaf[leafId].erase(_congestionToLeaf[leafId].begin());
    // }
}

void LeafSwitch::updateCongestionFromLeaf(int leafId, int congestionLevel) {
    // _congestionFromLeaf[leafId] = congestionLevel;
}

int LeafSwitch::selectBestUplink(int destLeafId) {
    // 尋找擁塞程度最小的上行鏈路
    // int bestUplink = 0;
    // int minCongestion = CONGA_MAX_CONGESTION + 1;
    
    // for(int uplink = 0; uplink < NUMOFCORE; uplink++) {
    //     // 獲取該路徑的擁塞值
    //     int pathCongestion = std::max(_congestionToLeaf[destLeafId][uplink], _congestionFromLeaf[destLeafId]);
    //     if(pathCongestion < minCongestion) {
    //         minCongestion = pathCongestion;
    //         bestUplink = uplink;
    //     }
    // }
    
    // return bestUplink;
}

int LeafSwitch::measureLocalCongestion(Queue* queue) {
    // 根據隊列使用率計算本地擁塞程度
    // double queueUtil = (double)queue->_queuesize / queue->_maxsize;
    // return quantizeCongestion(queueUtil);
}


void LeafSwitch::initializeCongestionToLeaf(int coreId) {
    // Initialize congestion values for this core path to all other leaves
    // for(int leafId = 0; leafId < 24; leafId++) {
    //     if(leafId != _id) {  // Don't need path to self
    //         if(_congestionToLeaf.find(leafId) == _congestionToLeaf.end()) {
    //             _congestionToLeaf[leafId] = std::vector<int>(12, 0);
    //         }
    //         _congestionToLeaf[leafId][coreId] = 0;  // Initialize congestion level to 0
    //     }
    // }
}

