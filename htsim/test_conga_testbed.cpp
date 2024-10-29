#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "aprx-fairqueue.h"
#include "fairqueue.h"
#include "priorityqueue.h"
#include "stoc-fairqueue.h"
#include "flow-generator.h"
#include "pipe.h"
#include "test.h"
#include "leafswitch.h"

namespace conga {
    // tesdbed configuration
    // number of node
    const int NUM_CORE = 12;
    const int NUM_LEAF = 24;
    const int NUM_SERVER = 32;                 // Per leaf so total server is 24 * 32 = 768
    // node's configure
    const uint64_t LEAF_BUFFER = 512000;
    const uint64_t CORE_BUFFER = 1024000;
    const uint64_t ENDH_BUFFER = 8192000;
    // link's configure
    const uint64_t CORE_SPEED = 40000000000;   // core <--> leaf   (40gbps)  
    const uint64_t LEAF_SPEED = 10000000000;   // leaf <--> server (10gbps)
    const double   LINK_DELAY = 0.1;           // in microsec


    //core switch
    Queue* qCoreLeaf[NUM_CORE][NUM_LEAF];      // Core to leaf queue
    Pipe* pCoreLeaf[NUM_CORE][NUM_LEAF];       // Core to leaf pipe
    //leaf switch
    Pipe* pLeafCore[NUM_LEAF][NUM_CORE];       // Leaf to core pipe
    Queue* qLeafCore[NUM_LEAF][NUM_CORE];      // Leaf to core queue
    std::vector<LeafSwitch*> leafSwitches;     // include two table and can decide where are flow go.
    Queue* qLeafServer[NUM_LEAF][NUM_SERVER];  // Leaf to server queue
    Pipe* pLeafServer[NUM_LEAF][NUM_SERVER];   // Leaf to server pipe
    //server (host)
    Pipe* pServerLeaf[NUM_LEAF][NUM_SERVER];   // Server to leaf pipe
    Queue* qServerLeaf[NUM_LEAF][NUM_SERVER];  // Server to leaf queue

    void generateRoute(route_t*& fwd, route_t*& rev, uint32_t& src, uint32_t& dst, uint64_t flowSize,  simtime_picosec startTime);


}

using namespace std;
using namespace conga;


void
conga_testbed(const ArgList &args, Logfile &logfile)
{
    // build leaf_switches
    for(int i = 0; i < NUM_LEAF; i++) {
        leafSwitches.push_back(new LeafSwitch(i));
    }

    // Core to leaf links implementation
    for(int i = 0; i < NUM_CORE; i++) {
        for(int j = 0; j < NUM_LEAF; j++) {
            // Create uplink queue (leaf to core)
            Queue* upQueue = new Queue(conga::CORE_SPEED, conga::CORE_BUFFER, NULL);
            upQueue->setName("q-leaf-core-" + std::to_string(j) + "-" + std::to_string(i));
            logfile.writeName(*upQueue);

            // Create uplink pipe
            Pipe* upPipe = new Pipe(timeFromUs(LINK_DELAY));
            upPipe->setName("p-leaf-core-" + std::to_string(j) + "-" + std::to_string(i));
            logfile.writeName(*upPipe);

            // Create downlink queue (core to leaf)
            Queue* downQueue = new Queue(conga::CORE_SPEED, conga::CORE_BUFFER, NULL);
            downQueue->setName("q-core-leaf-" + std::to_string(i) + "-" + std::to_string(j));
            logfile.writeName(*downQueue);

            // Create downlink pipe
            Pipe* downPipe = new Pipe(timeFromUs(LINK_DELAY));
            downPipe->setName("p-core-leaf-" + std::to_string(i) + "-" + std::to_string(j));
            logfile.writeName(*downPipe);

            // Store queues and pipes in global arrays for later use
            qCoreLeaf[i][j] = downQueue;  // Core to leaf queue
            pCoreLeaf[i][j] = downPipe;   // Core to leaf pipe

            qLeafCore[j][i] = upQueue;    // Leaf to core queue
            pLeafCore[j][i] = upPipe;     // Leaf to core pipe
            
            // Update leaf switch congestion tables
            leafSwitches[j]->initializeCongestionToLeaf(i);
        }
    }

    // Leaf to server links implementation
    for(int i = 0; i < NUM_LEAF; i++) {
        for(int j = 0; j < NUM_SERVER; j++) {
            // Create server uplink queue (server to leaf)
            Queue* upQueue = new Queue(conga::LEAF_SPEED, conga::LEAF_BUFFER, NULL);
            upQueue->setName("q-server-leaf-" + std::to_string(i) + "-" + std::to_string(j));
            logfile.writeName(*upQueue);

            // Create server uplink pipe
            Pipe* upPipe = new Pipe(timeFromUs(LINK_DELAY));
            upPipe->setName("p-server-leaf-" + std::to_string(i) + "-" + std::to_string(j));
            logfile.writeName(*upPipe);

            // Create server downlink queue (leaf to server)
            Queue* downQueue = new Queue(conga::LEAF_SPEED, conga::LEAF_BUFFER, NULL);
            downQueue->setName("q-leaf-server-" + std::to_string(i) + "-" + std::to_string(j));
            logfile.writeName(*downQueue);

            // Create server downlink pipe
            Pipe* downPipe = new Pipe(timeFromUs(LINK_DELAY));
            downPipe->setName("p-leaf-server-" + std::to_string(i) + "-" + std::to_string(j));
            logfile.writeName(*downPipe);

            // Store queues and pipes in global arrays for later use
            qServerLeaf[i][j] = upQueue;    // Server to leaf queue
            pServerLeaf[i][j] = upPipe;     // Server to leaf pipe
            
            qLeafServer[i][j] = downQueue;  // Leaf to server queue
            pLeafServer[i][j] = downPipe;   // Leaf to server pipe

            // Create server node ID
            uint32_t serverId = i * NUM_SERVER + j;
            
            // Optional: Initialize any server-specific configurations in leaf switch
            //leafSwitches[i]->addConnectedServer(serverId);
        }
    }

    //deal with workload
    string FlowDist = "uniform";
    parseString(args, "flowdist", FlowDist);
    Workloads::FlowDist fd  = Workloads::UNIFORM; //用0,1,2,3代表workload模式
    if (FlowDist == "pareto") {
        fd = Workloads::PARETO;
    } else if (FlowDist == "enterprise") {
        fd = Workloads::ENTERPRISE;
    } else if (FlowDist == "datamining") {
        fd = Workloads::DATAMINING;
    }

    uint32_t Duration = 5;
    double Utilization = 0.8;
    uint32_t AvgFlowSize = 100000;

    parseInt(args, "duration", Duration);
    parseInt(args, "flowsize", AvgFlowSize);
    parseDouble(args, "utilization", Utilization);


    double totalBandwidth = NUM_CORE * CORE_SPEED;
    double flowRate = totalBandwidth * Utilization;

    FlowGenerator* flowGen = new FlowGenerator(
        DataSource::TCP,  
        generateRoute,      
        flowRate,          
        AvgFlowSize,       
        fd                 
    );

    flowGen->setTimeLimits(timeFromUs(1), timeFromSec(Duration) - 1);
    EventList::Get().setEndtime(timeFromSec(Duration));

}

void conga::generateRoute(route_t*& fwd, route_t*& rev, uint32_t& src, uint32_t& dst, uint64_t flowSize, 
                          simtime_picosec startTime) {
    fwd = new route_t();
    rev = new route_t();
    int numOfNode = NUM_LEAF * NUM_SERVER;
     if (dst != 0) {
        dst = dst % numOfNode;
    } else {
        dst = rand() % numOfNode;
    }

    if (src != 0) {
        src = src % (numOfNode - 1);
    } else {
        src = rand() % (numOfNode - 1);
    }

    if (src >= dst) {
        src++;
    }

    //printf("src{%d}, dst{%d}", src, dst);
    uint32_t src_leaf = src / conga::NUM_SERVER;
    uint32_t dst_leaf = dst / conga::NUM_SERVER;
    uint32_t src_server = src % conga::NUM_SERVER;
    uint32_t dst_server = dst % conga::NUM_SERVER;

    
    
    //update congestion table=============================
    // simtime_picosec timeDiff = startTime - leafSwitches[src_leaf]->timeStamp;
    // int updateIter = timeDiff / 10000000;
    // for(int j = 0; j < NUM_CORE; j++) {
    //     for(int k = 0; k < updateIter; k++) 
    //         leafSwitches[src_leaf]->uplink[j] *= 0.2;
    //         leafSwitches[src_leaf]->downlink[j] *= 0.2;
    // }
    // leafSwitches[src_leaf]->timeStamp = startTime;   
    
    // timeDiff = startTime - leafSwitches[dst_leaf]->timeStamp;
    // updateIter = timeDiff / 10000000;
    // for(int j = 0; j < NUM_CORE; j++) {
    //     for(int k = 0; k < updateIter; k++) 
    //         leafSwitches[dst_leaf]->uplink[j] *= 0.2;
    //         leafSwitches[dst_leaf]->downlink[j] *= 0.2;
    // }
    // leafSwitches[dst_leaf]->timeStamp = startTime;  

    
    // build network topology
    uint32_t initial_core = 0;
    long long minCongestion = LEAF_BUFFER;

    for (int i = 0; i < NUM_CORE; i++) {
        // long long curCongestion = max(qLeafCore[src_leaf][i]->_flowCapacity, qCoreLeaf[i][dst_leaf]->_flowCapacity);
        long long curCongestion = max(qLeafCore[src_leaf][i]->_queuesize, qCoreLeaf[i][dst_leaf]->_queuesize);
        if(curCongestion < minCongestion) {
            minCongestion = curCongestion;
            initial_core = i;
        } 
    }
    // initial_core = (src_leaf+dst_leaf) % NUM_CORE;

    fwd->push_back(qServerLeaf[src_leaf][src_server]);
    fwd->push_back(pServerLeaf[src_leaf][src_server]);

    //====================================================
    if (src_leaf != dst_leaf) {
        fwd->push_back(qLeafCore[src_leaf][initial_core]);
        fwd->push_back(pLeafCore[src_leaf][initial_core]);
        
        fwd->push_back(qCoreLeaf[initial_core][dst_leaf]);
        fwd->push_back(pCoreLeaf[initial_core][dst_leaf]);
    }

    fwd->push_back(qLeafServer[dst_leaf][dst_server]);
    fwd->push_back(pLeafServer[dst_leaf][dst_server]);
    
    //===============================================================
    rev->push_back(qServerLeaf[dst_leaf][dst_server]);
    rev->push_back(pServerLeaf[dst_leaf][dst_server]);

    if (src_leaf != dst_leaf) {
        fwd->push_back(qLeafCore[dst_leaf][initial_core]);
        fwd->push_back(pLeafCore[dst_leaf][initial_core]);

        fwd->push_back(qCoreLeaf[initial_core][src_leaf]);
        fwd->push_back(pCoreLeaf[initial_core][src_leaf]);
    }

    rev->push_back(qLeafServer[src_leaf][src_server]);
    rev->push_back(pLeafServer[src_leaf][src_server]);
}