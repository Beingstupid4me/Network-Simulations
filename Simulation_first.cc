#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FiveRouterTopology");

void PacketSent (Ptr<const Packet> packet)

{

NS_LOG_UNCOND ("Packet sent at time " << Simulator::Now ().GetSeconds ());

}

void PacketReceived (Ptr<const Packet> packet)

{

NS_LOG_UNCOND ("Packet received at time " << Simulator::Now ().GetSeconds ());

}
void PrintStatistics(Ptr<FlowMonitor> monitor, FlowMonitorHelper &flowHelper)
{
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());

    for (auto const &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow ID: " << flow.first << "\n";
        std::cout << "Source: " << t.sourceAddress << " --> Destination: " << t.destinationAddress << "\n";
        std::cout << "Tx Packets: " << flow.second.txPackets << ", Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "Lost Packets: " << (flow.second.txPackets - flow.second.rxPackets) << "\n";
        std::cout << "Average Delay: " << (flow.second.delaySum.GetSeconds() / flow.second.rxPackets) << " s\n";
        std::cout << "Throughput: " << (flow.second.rxBytes * 8.0 / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds())) / 1024 << " Kbps\n\n";
    }
}

int main(int argc, char *argv[])
{
    // Log configuration
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    double error_rate = 0.001;
    // string delay = "5ms";
    // Create nodes
    NodeContainer routers;
    routers.Create(5); // R1, R2, R3, R4, R5

    NodeContainer endPoints[5];
    for (int i = 0; i < 5; ++i)
    {
        endPoints[i].Create(1);
    }

    // Define P2P links and properties
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    // propagation delay
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    // Install links between routers
    NetDeviceContainer routerDevices[10];
    routerDevices[0] = p2p.Install(NodeContainer(routers.Get(0), routers.Get(1))); // R1-R2
    routerDevices[1] = p2p.Install(NodeContainer(routers.Get(1), routers.Get(2))); // R2-R3
    routerDevices[2] = p2p.Install(NodeContainer(routers.Get(2), routers.Get(3))); // R3-R4
    routerDevices[3] = p2p.Install(NodeContainer(routers.Get(3), routers.Get(4))); // R4-R5
    routerDevices[4] = p2p.Install(NodeContainer(routers.Get(4), routers.Get(0))); // R5-R1
    Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
    em->SetAttribute ("ErrorRate", DoubleValue (error_rate));

    for (int i=0;i<5;i++){
      routerDevices[i].Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
    }
    
    // Connect endpoints to routers
    NetDeviceContainer endPointDevices[5];
    for (int i = 0; i < 5; ++i)
    {
        endPointDevices[i] = p2p.Install(NodeContainer(endPoints[i].Get(0), routers.Get(i)));
        endPointDevices[i].Get(1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
    }

    // Install internet stack
    InternetStackHelper stack;
    for (int i = 0; i < 5; ++i)
    {
        stack.Install(endPoints[i]);
    }
    stack.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer routerInterfaces[10];
    for (int i = 0; i < 5; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        routerInterfaces[i] = address.Assign(routerDevices[i]);
    }

    Ipv4InterfaceContainer endPointInterfaces[5];
    for (int i = 0; i < 5; ++i)
    {
        std::ostringstream subnet;
        subnet << "192.168." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        endPointInterfaces[i] = address.Assign(endPointDevices[i]);
    }

    // Install applications: UDP Echo Server and Client
    UdpEchoServerHelper echoServer(9);

    for (int i = 0; i < 5; ++i)
    {
        ApplicationContainer serverApps = echoServer.Install(endPoints[i].Get(0));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(60.0));
    }

    UdpEchoClientHelper echoClient(endPointInterfaces[4].GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(endPoints[0].Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(60.0));

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable pcap tracing
    p2p.EnablePcapAll("five-router");

    // Flow monitor to gather statistics
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    // enable tracing 
    routerDevices[0].Get (0)->TraceConnectWithoutContext ("MacTx", MakeCallback (&PacketSent));
    routerDevices[1].Get (0)->TraceConnectWithoutContext ("MacRx", MakeCallback (&PacketReceived));

    // Run simulation
    Simulator::Stop(Seconds(60.0));
    Simulator::Run();

    // Print statistics
    PrintStatistics(monitor, flowHelper);
    monitor->SerializeToXmlFile("flow-monitor.xml", true, true);

    Simulator::Destroy();
    return 0;
}
