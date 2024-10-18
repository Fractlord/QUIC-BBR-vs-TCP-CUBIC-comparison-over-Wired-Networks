
/* 
===================================================================
    Client-Router-Server Point-to-Point Topology
===================================================================

        +--------+          +---------+          +--------+
        | Client |----------| Router  |----------| Server |
        +--------+          +---------+          +--------+

    - Client, Router, and Server are connected via point-to-point links.
    - TCP CUBIC is simulated on the client-server path.
    - The router forwards packets between the client and server.

    Topology configuration:
    ------------------------
    Link 1 (Client <-> Router)
    Link 2 (Router <-> Server)
    
===================================================================
*/





#include <iomanip>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

#define TCP_SEGMENT_SIZE 1500
#define DATA_RATE1 "5Mbps"
#define DATA_RATE2 "5Mbps"
#define DURATION 100.0

using namespace ns3;

Ptr<PacketSink> sink;
uint64_t lastTotalRx = 0;
uint32_t packetsSent = 0;
uint32_t packetsReceived = 0;
std::ofstream cwndFile, rttFile, throughputFile, packetLossFile;

// Track packets sent
void PacketSentCallback(Ptr<const Packet> packet) {
    packetsSent++;
}

// Track packets received
void PacketReceivedCallback(Ptr<const Packet> packet, const Address &address) {
    packetsReceived++;
}

// Cwnd change function (in packets)
static void CwndChange(uint32_t oldCwnd, uint32_t newCwnd) {
    double time = Simulator::Now().GetSeconds();
    double cwndInPackets = newCwnd / TCP_SEGMENT_SIZE;  // Convert to packets
    cwndFile << time << " " << cwndInPackets << std::endl;
}

// RTT change function
static void RttChange(Time oldRtt, Time newRtt) {
    double time = Simulator::Now().GetSeconds();
    rttFile << time << " " << newRtt.GetMilliSeconds() << std::endl;
}

// Throughput calculation function
static void findThroughput() {
    Time currentTime = Simulator::Now();
    double time = currentTime.GetSeconds();
    double currentThroughput = (sink->GetTotalRx() - lastTotalRx) * 8.0 / 1e6;  // Converted to Mbps
    throughputFile << time << " " << currentThroughput << std::endl;
    lastTotalRx = sink->GetTotalRx();
    Simulator::Schedule(MilliSeconds(100), &findThroughput);
}

// Packet loss calculation function
static void CalculatePacketLoss() {
    double time = Simulator::Now().GetSeconds();
    if (packetsSent > 0) {
        double packetLossRate = ((packetsSent - packetsReceived) / static_cast<double>(packetsSent)) * 100;
        packetLossFile << time << " " << packetLossRate << std::endl;
    } else {
        packetLossFile << time << " " << 0.0 << std::endl;
    }
    Simulator::Schedule(MilliSeconds(100), &CalculatePacketLoss);
}

static void TraceCwnd() {
    Config::ConnectWithoutContext("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow", MakeCallback(&CwndChange));
}

static void TraceRtt() {
    Config::ConnectWithoutContext("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/RTT", MakeCallback(&RttChange));
}

int main() {
    int tcpSegmentSize = TCP_SEGMENT_SIZE;
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(tcpSegmentSize));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(2));
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpCubic"));

    NodeContainer nodes;
    nodes.Create(3);

    Ptr<Node> client = nodes.Get(0);
    Ptr<Node> router = nodes.Get(1);
    Ptr<Node> server = nodes.Get(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(DATA_RATE2));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer clientRouterDevices = pointToPoint.Install(client, router);
    NetDeviceContainer routerServerDevices = pointToPoint.Install(router, server);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer clientRouterInterfaces = address.Assign(clientRouterDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer routerServerInterfaces = address.Assign(routerServerDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t serverPort = 9;

    Address sinkAddr(InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddr);
    ApplicationContainer sinkApp = sinkHelper.Install(server);
    sinkApp.Start(Seconds(0.01));
    sinkApp.Stop(Seconds(DURATION));
    sink = DynamicCast<PacketSink>(sinkApp.Get(0));

    // BulkSendApplication setup to send data
    BulkSendHelper sourceHelper("ns3::TcpSocketFactory", InetSocketAddress(routerServerInterfaces.GetAddress(1), serverPort));
    sourceHelper.SetAttribute("MaxBytes", UintegerValue(0));  // Send unlimited data
    ApplicationContainer sourceApp = sourceHelper.Install(client);
    sourceApp.Start(Seconds(0.0));
    sourceApp.Stop(Seconds(DURATION));

    // Open the output files
    std::string outputDir = "/source/path/forns3/desired/output/file/"; //CHANGE THIS 
    cwndFile.open(outputDir + "tcpcubic.cwnd");
    rttFile.open(outputDir + "tcpcubic.rtt");
    throughputFile.open(outputDir + "tcpcubic.throughput");
    packetLossFile.open(outputDir + "tcpcubic.packetloss");
    if (!cwndFile.is_open() || !rttFile.is_open() || !throughputFile.is_open() || !packetLossFile.is_open()) {
        std::cerr << "Error opening output files" << std::endl;
        return 1;
    }

    // Schedule tracing functions
    Simulator::Schedule(Seconds(0.01), &TraceCwnd);
    Simulator::Schedule(Seconds(0.01), &TraceRtt);
    Simulator::Schedule(Seconds(1.0), &findThroughput);
    Simulator::Schedule(Seconds(1.0), &CalculatePacketLoss);

    // Connect callbacks for packet tracking
    sourceApp.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&PacketSentCallback));
    sinkApp.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&PacketReceivedCallback));

    Simulator::Stop(Seconds(DURATION));
    Simulator::Run();

    // Close the output files
    cwndFile.close();
    rttFile.close();
    throughputFile.close();
    packetLossFile.close();

    std::cout << "Total Bytes Received from Client: " << sink->GetTotalRx() << std::endl;

    Simulator::Destroy();

    return 0;
}

