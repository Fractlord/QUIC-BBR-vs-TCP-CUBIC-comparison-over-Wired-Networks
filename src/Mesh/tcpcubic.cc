/* 
===================================================================
                        Mesh Topology (10 Nodes)
===================================================================

       +--------+       +--------+       +--------+       +--------+
       | Node 1 |-------| Node 2 |-------| Node 3 |-------| Node 4 |
       +--------+       +--------+       +--------+       +--------+
          \  |  /          \  |  /          \  |  /          \  |  /
          +--------+       +--------+       +--------+       +--------+
          | Node 5 |-------| Node 6 |-------| Node 7 |-------| Node 8 |
          +--------+       +--------+       +--------+       +--------+
             \  |  /          \  |  /          \  |  /          \  |  /
             +--------+       +--------+
             | Node 9 |-------| Server |
             +--------+       +--------+

    - 10 nodes connected in a fully meshed topology.
    - Each node is connected to every other node in the network.
    - All links are point-to-point with the following characteristics:
      - Data Rate: 6 Mbps for each link
      - Delay: 15 ms for each link

===================================================================
*/





#include <iomanip>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

#define TCP_SEGMENT_SIZE 1500
#define DATA_RATE "18Mbps"
#define MESH_DATA_RATE "6Mbps"
#define MESH_DELAY "15ms"
#define DURATION 100.0
#define NUM_NODES 10

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

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    int tcpSegmentSize = TCP_SEGMENT_SIZE;
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(tcpSegmentSize));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(2));
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpCubic"));

    NodeContainer nodes;
    nodes.Create(NUM_NODES);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(MESH_DATA_RATE));
    pointToPoint.SetChannelAttribute("Delay", StringValue(MESH_DELAY));

    NetDeviceContainer devices;
    InternetStackHelper stack;
    stack.Install(nodes);
    Ipv4AddressHelper address;

    // Create a mesh topology
    int subnet = 1;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        for (uint32_t j = i + 1; j < nodes.GetN(); ++j) {
            NetDeviceContainer link = pointToPoint.Install(NodeContainer(nodes.Get(i), nodes.Get(j)));
            devices.Add(link);
            std::ostringstream subnetStream;
            subnetStream << "10.1." << subnet++ << ".0";
            address.SetBase(subnetStream.str().c_str(), "255.255.255.0");
            address.Assign(link);
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t serverPort = 9;

    Address sinkAddr(InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddr);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(NUM_NODES - 1)); // Install on the last node
    sinkApp.Start(Seconds(0.01));
    sinkApp.Stop(Seconds(DURATION));
    sink = DynamicCast<PacketSink>(sinkApp.Get(0));

    // Assuming the server node is the last one, we need to find its IP
    Ptr<Ipv4> ipv4 = nodes.Get(NUM_NODES - 1)->GetObject<Ipv4>();
    Ipv4Address serverIp = ipv4->GetAddress(1, 0).GetLocal(); // Get the IP of the last node

    BulkSendHelper sourceHelper("ns3::TcpSocketFactory", InetSocketAddress(serverIp, serverPort));
    sourceHelper.SetAttribute("MaxBytes", UintegerValue(0));  // Send unlimited data
    ApplicationContainer sourceApp = sourceHelper.Install(nodes.Get(0)); // Install on the first node
    sourceApp.Start(Seconds(0.0));
    sourceApp.Stop(Seconds(DURATION));

    // Open the output files
    std::string outputDir = "/path/to/sourcens3/folder/desired/output/file/"; //CHANGE THIS
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
