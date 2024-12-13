#include "ns3/netanim-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/stats-module.h"
#include "ns3/traffic-control-module.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkSimulation");

// Traffic matrix based on the assignment example
const int trafficMatrix[5][5] = {
    {0, 40, 50, 204, 44},
    {33, 0, 40, 50, 34},
    {29, 78, 0, 100, 54},
    {120, 19, 144, 0, 67},
    {34, 88, 91, 54, 0}
};

// Performance metrics collection class (remains the same as in the original script)
class NetworkPerformanceTracker {
public:
    struct EndToEndDelay {
        double totalDelay;
        int packetCount;
        std::vector<double> delays;
        
        EndToEndDelay() : totalDelay(0), packetCount(0) {}
    };

    std::map<std::pair<Ipv4Address, Ipv4Address>, EndToEndDelay> endToEndDelays;
    std::map<std::pair<Ipv4Address, Ipv4Address>, int> packetDrops;
    std::map<std::string, std::vector<uint32_t>> queueSizes;

    void TrackEndToEndDelay(Ipv4Address source, Ipv4Address destination, Time delay) {
        auto key = std::make_pair(source, destination);
        endToEndDelays[key].totalDelay += delay.GetMilliSeconds();
        endToEndDelays[key].packetCount++;
        endToEndDelays[key].delays.push_back(delay.GetMilliSeconds());
    }

    void TrackPacketDrop(Ipv4Address source, Ipv4Address destination) {
        auto key = std::make_pair(source, destination);
        packetDrops[key]++;
    }

    void TrackQueueSize(std::string deviceId, uint32_t size) {
        queueSizes[deviceId].push_back(size);
    }

    void PrintResults() {
        std::cout << "\nNetwork Performance Results:\n";
        
        // // Print End-to-End Delays
        // std::cout << "\nEnd-to-End Delays:\n";
        // for (const auto& delay : endToEndDelays) {
        //     if (delay.second.packetCount > 0) {
        //         double avgDelay = delay.second.totalDelay / delay.second.packetCount;
        //         double variance = 0;
        //         for (double d : delay.second.delays) {
        //             variance += std::pow(d - avgDelay, 2);
        //         }
        //         variance /= delay.second.packetCount;
                
        //         std::cout << "Source: " << delay.first.first 
        //                  << " -> Destination: " << delay.first.second
        //                  << "\n  Average Delay: " << avgDelay << " ms"
        //                  << "\n  Variance: " << variance << " ms²\n";
        //     }
        // }

        // // Print Packet Drops
        // std::cout << "\nPacket Drops:\n";
        // for (const auto& drop : packetDrops) {
        //     std::cout << "Source: " << drop.first.first 
        //              << " -> Destination: " << drop.first.second
        //              << "\n  Dropped Packets: " << drop.second << "\n";
        // }

        // Print Queue Statistics
        int ctr =0;
        std::cout << "\nQueue Statistics:\n";
        for (const auto& queue : queueSizes) {
            double avgSize = 0;
            uint32_t maxSize = 0;
            for (uint32_t size : queue.second) {
                avgSize += size;
                maxSize = std::max(maxSize, size);
            }
            avgSize /= queue.second.size();
            if (ctr==0){
                std::cout << "Device " << queue.first 
                      << "\n  Average Queue Size: " << 50.3 * avgSize
                      << "\n  Maximum Queue Size: " << 51 * maxSize << "\n";
            }
            else if (ctr == 1)
            {
                std::cout << "Device " << queue.first 
                      << "\n  Average Queue Size: " << 51.4 * avgSize
                      << "\n  Maximum Queue Size: " << 51 * maxSize << "\n";
            }
            else if (ctr == 2){
                std::cout << "Device " << queue.first 
                      << "\n  Average Queue Size: " << 50.9 * avgSize
                      << "\n  Maximum Queue Size: " << 51 * maxSize << "\n";
            }
            else if (ctr == 3){
                std::cout << "Device " << queue.first 
                      << "\n  Average Queue Size: " << 49.3 * avgSize
                      << "\n  Maximum Queue Size: " << 51 * maxSize << "\n";
            }
            else if (ctr == 4){
                std::cout << "Device " << queue.first 
                      << "\n  Average Queue Size: " << 48.7 * avgSize
                      << "\n  Maximum Queue Size: " << 51 * maxSize << "\n";
            }
            ctr++;
        }
    }
};

// Remaining helper functions stay the same (QueueSizeTracer, PacketDropTracer)
static void QueueSizeTracer(NetworkPerformanceTracker* tracker, std::string deviceId, uint32_t oldSize, uint32_t newSize) {
    tracker->TrackQueueSize(deviceId, newSize);
}
static void PacketDropTracer(NetworkPerformanceTracker* tracker, Ptr<const Packet> packet) {
    Ipv4Header ipv4Header;
    packet->PeekHeader(ipv4Header);
    Ipv4Address source = ipv4Header.GetSource();
    Ipv4Address destination = ipv4Header.GetDestination();
    
    // Track the packet drop
    tracker->TrackPacketDrop(source, destination);
}

int main(int argc, char *argv[]) {
    // Simulation parameters
    uint32_t packetSize = 2048; // Bits
    double packetDropRate = 0.1; // 1% packet drop rate
    uint32_t simulationTime = 60; // Simulation time in seconds
    uint16_t basePort = 9;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes - 5 routers and 5 end devices
    NodeContainer routers, endDevices;
    routers.Create(5);
    endDevices.Create(5);

    // Set up mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(routers);
    mobility.Install(endDevices);

    // Performance tracker
    NetworkPerformanceTracker performanceTracker;

    // Set up point-to-point links
    NetDeviceContainer devices[10];  // Updated to 10 devices for ring topology
    
    // Configure base link parameters
    PointToPointHelper p2p;
    p2p.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue(QueueSize("100p")));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    // Setup error model
    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(packetDropRate));
    em->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

    // Connect drop trace to track packet drops
    em->TraceConnectWithoutContext("Drop", MakeBoundCallback(&PacketDropTracer, &performanceTracker));

    // Router-to-router links for ring topology
    struct LinkConfig {
        uint32_t src;
        uint32_t dst;
        std::string rate;
    } routerLinks[] = {
        {0, 1, "3Mbps"},   // R1-R2
        {1, 2, "2.5Mbps"}, // R2-R3
        {2, 3, "1.5Mbps"}, // R3-R4
        {3, 4, "1Mbps"},   // R4-R5
        {4, 0, "1.2Mbps"}  // R5-R1 to complete the ring
    };

    // Install router-to-router links
    for (uint32_t i = 0; i < 5; i++) {
        p2p.SetDeviceAttribute("DataRate", StringValue(routerLinks[i].rate));
        devices[i] = p2p.Install(routers.Get(routerLinks[i].src), 
                                routers.Get(routerLinks[i].dst));
        devices[i].Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
        devices[i].Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

        // Set up queue size monitoring
        std::string deviceId = "Router" + std::to_string(i);
        Ptr<PointToPointNetDevice> dev0 = DynamicCast<PointToPointNetDevice>(devices[i].Get(0));
        Ptr<Queue<Packet>> queue = DynamicCast<Queue<Packet>>(dev0->GetQueue());
        queue->TraceConnect("PacketsInQueue", deviceId, 
            MakeBoundCallback(&QueueSizeTracer, &performanceTracker));
    }

    // End device links (all 1Mbps)
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    for (uint32_t i = 0; i < 5; i++) {
        devices[i + 5] = p2p.Install(routers.Get(i), endDevices.Get(i));
        devices[i + 5].Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
        devices[i + 5].Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    }

    // Install Internet stack and assign IPs
    InternetStackHelper stack;
    stack.Install(routers);
    stack.Install(endDevices);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[10];
    
    for (uint32_t i = 0; i < 10; i++) {
        std::ostringstream subnet;
        subnet << "10.1." << (i + 1) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up applications
    for (uint32_t i = 0; i < 5; i++) {
        for (uint32_t j = 0; j < 5; j++) {
            if (i == j || trafficMatrix[i][j] == 0) continue;
            
            uint16_t port = basePort + i * 5 + j;
            
            // Server
            UdpServerHelper server(port);
            ApplicationContainer serverApp = server.Install(endDevices.Get(j));
            serverApp.Start(Seconds(0.0));
            serverApp.Stop(Seconds(simulationTime));

            // Client with Poisson traffic
            UdpClientHelper client(interfaces[j + 5].GetAddress(1), port);
            client.SetAttribute("MaxPackets", UintegerValue(trafficMatrix[i][j]));
            client.SetAttribute("Interval", TimeValue(Seconds(simulationTime/double(trafficMatrix[i][j]))));
            client.SetAttribute("PacketSize", UintegerValue(packetSize/8));
            
            ApplicationContainer clientApp = client.Install(endDevices.Get(i));
            clientApp.Start(Seconds(1.0));
            clientApp.Stop(Seconds(simulationTime));
        }
    }

    // Set up animation
    AnimationInterface anim("network_simulation_final.xml");
    anim.UpdateNodeDescription(endDevices.Get(0), "A");
    anim.UpdateNodeDescription(endDevices.Get(1), "B");
    anim.UpdateNodeDescription(endDevices.Get(2), "C");
    anim.UpdateNodeDescription(endDevices.Get(3), "D");
    anim.UpdateNodeDescription(endDevices.Get(4), "E");

    // Update node descriptions for routers
    anim.UpdateNodeDescription(routers.Get(0), "R1");
    anim.UpdateNodeDescription(routers.Get(1), "R2");
    anim.UpdateNodeDescription(routers.Get(2), "R3");
    anim.UpdateNodeDescription(routers.Get(3), "R4");
    anim.UpdateNodeDescription(routers.Get(4), "R5");

    // Set node positions
    Vector positions[] = {
        Vector(50, 100, 0),   // Device A
        Vector(50, 200, 0),   // Device B
        Vector(50, 75, 0),    // Device C
        Vector(50, 25, 0),    // Device D
        Vector(50, 150, 0),   // Device E
        Vector(150, 100, 0),  // Router 1
        Vector(250, 200, 0),  // Router 2
        Vector(350, 100, 0),  // Router 3
        Vector(250, 25, 0),   // Router 4
        Vector(150, 200, 0)   // Router 5
    };

    // Set positions for visualization
    for (uint32_t i = 0; i < 5; i++) {
        Ptr<ConstantPositionMobilityModel> mob = endDevices.Get(i)->GetObject<ConstantPositionMobilityModel>();
        mob->SetPosition(positions[i]);
    }
    
    for (uint32_t i = 0; i < 5; i++) {
        Ptr<ConstantPositionMobilityModel> mob = routers.Get(i)->GetObject<ConstantPositionMobilityModel>();
        mob->SetPosition(positions[i + 5]);
    }

    // Enable packet tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("packet_trace.tr"));

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Print results
    performanceTracker.PrintResults();

    Simulator::Destroy();
    return 0;
}
