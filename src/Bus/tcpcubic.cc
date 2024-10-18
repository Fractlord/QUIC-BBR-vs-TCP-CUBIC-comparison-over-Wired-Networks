/* 
===================================================================
                        Bus Topology (6 Nodes)
===================================================================

+--------+      +--------+      +--------+      +--------+      +--------+      +--------+
| Client |------| Client |------| Client |------| Client |------| Client |------| Server |
+--------+      +--------+      +--------+      +--------+      +--------+      +--------+

    - 5 Clients are connected in a bus topology using CSMA (Carrier Sense Multiple Access).
    - All clients share a common communication medium (bus).
    - The server is located at the end of the bus, receiving data from the clients.
    - The simulation tracks TCP CUBIC performance using the following metrics:
      
    - Data Rate: 85 Mbps on the CSMA bus
    - Delay: 3 ms propagation delay for all links

===================================================================
*/



#include <iomanip>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

#define TCP_SEGMENT_SIZE 1500
#define DATA_RATE "135Mbps"         // Adjusted data rate for modern high-speed networks
#define CSMA_DATA_RATE "85Mbps"    // Adjusted to simulate a modern shared bus capacity
#define CSMA_DELAY "3ms"          // Adjusted for minimal realistic propagation delay
#define DURATION 100.0
#define NUM_NODES 6

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
    Simulator::Schedule(Seconds(1.0), &findThroughput);  // Recalculate every 1 second
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
    Simulator::Schedule(Seconds(1.0), &CalculatePacketLoss);  // Recalculate every 1 second
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

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue(CSMA_DATA_RATE));
    csma.SetChannelAttribute("Delay", StringValue(CSMA_DELAY));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t serverPort = 9;
    Address sinkAddr(InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddr);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(NUM_NODES - 1));
    sinkApp.Start(Seconds(0.01));
    sinkApp.Stop(Seconds(DURATION));
    sink = DynamicCast<PacketSink>(sinkApp.Get(0));

    // Implement BulkSendApplication on each node
    for (uint32_t i = 0; i < NUM_NODES - 1; ++i) {
        BulkSendHelper sourceHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(interfaces.GetAddress(NUM_NODES - 1), serverPort));
        sourceHelper.SetAttribute("MaxBytes", UintegerValue(0));  // Send unlimited data
        ApplicationContainer sourceApp = sourceHelper.Install(nodes.Get(i));
        sourceApp.Start(Seconds(0.0));
        sourceApp.Stop(Seconds(DURATION));
    }

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
    Simulator::Schedule(Seconds(1.0), &TraceCwnd);
    Simulator::Schedule(Seconds(1.0), &TraceRtt);
    Simulator::Schedule(Seconds(1.0), &findThroughput);
    Simulator::Schedule(Seconds(1.0), &CalculatePacketLoss);

    // Connect callbacks for packet tracking
    for (uint32_t i = 0; i < NUM_NODES - 1; ++i) {
        nodes.Get(i)->GetApplication(0)->TraceConnectWithoutContext("Tx", MakeCallback(&PacketSentCallback));
    }
    sinkApp.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&PacketReceivedCallback));

    Simulator::Stop(Seconds(DURATION));
    Simulator::Run();

    // Close the output files
    cwndFile.close();
    rttFile.close();
    throughputFile.close();
    packetLossFile.close();

    std::cout << "Total Bytes Received from Server: " << sink->GetTotalRx() << std::endl;

    Simulator::Destroy();

    return 0;
} 
