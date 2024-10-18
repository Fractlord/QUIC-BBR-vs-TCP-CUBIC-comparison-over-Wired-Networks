/* 
===================================================================
                        Ring Topology (10 Nodes)
===================================================================

     +--------+      +--------+      +--------+      +--------+
     | Node 1 |------| Node 2 |------| Node 3 |------| Node 4 |
     +--------+      +--------+      +--------+      +--------+
         |                                    |            |
     +--------+      +--------+      +--------+      +--------+
     | Server |------| Node 10|------| Node 9 |------| Node 8 |
     +--------+      +--------+      +--------+      +--------+
                                    |       |
                              +--------+      +--------+
                              | Node 7 |------| Node 6 |
                              +--------+      +--------+

    - 10 nodes connected in a ring topology using point-to-point links.
    - The server is placed at Node 5.
    - All nodes are connected in a ring, and the last node connects to the first node, forming a closed loop.
    - The simulation tracks TCP CUBIC performance using the following metrics:
    
    - Data Rate: 5 Mbps for the links
    - Delay: 15 ms propagation delay for all point-to-point links

===================================================================
*/







#include <iomanip>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-socket-base.h"

#define TCP_SEGMENT_SIZE 1500  // Match QUIC packet size
#define DATA_RATE "5Mbps"      // Match QUIC data rate
#define RING_DATA_RATE "5Mbps" // Match QUIC ring data rate
#define RING_DELAY "15ms"      // Match QUIC ring delay
#define DURATION 100.0
#define NUM_NODES 10  // Changed to 10 nodes

using namespace ns3;

Ptr<PacketSink> sink;
uint64_t lastTotalRx = 0;
uint64_t totalPacketsSent = 0;
uint64_t totalPacketsReceived = 0;
std::ofstream cwndFile, rttFile, throughputFile, packetLossFile;

// Function to track packet transmissions (sent packets)
static void PacketSent(Ptr<const Packet> p) {
    totalPacketsSent++;
}

// Function to track packet receptions (received packets)
static void PacketReceived(Ptr<const Packet> p) {
    totalPacketsReceived++;
}

// Function to calculate and log packet loss percentage
static void CalculatePacketLoss() {
    double time = Simulator::Now().GetSeconds();
    if (totalPacketsSent > 0) {
        double packetLossPercent = (1 - (double)totalPacketsReceived / totalPacketsSent) * 100;
        packetLossFile << time << " " << packetLossPercent << std::endl;
        std::cout << std::setw(10) << "Time" << std::setw(25) << "Packet Loss (%)" << std::endl;
        std::cout << std::setw(10) << time << std::setw(25) << packetLossPercent << std::endl;
    }
    Simulator::Schedule(Seconds(1.0), &CalculatePacketLoss);  // Schedule to run every second
}

// Cwnd change function
static void CwndChange(uint32_t oldCwnd, uint32_t newCwnd) {
    double time = Simulator::Now().GetSeconds();
    double g_cwnd = newCwnd / TCP_SEGMENT_SIZE;  // Convert to packets
    cwndFile << time << " " << g_cwnd << std::endl;
    std::cout << std::setw(10) << "Time" << std::setw(15) << "Cwnd (Packets)" << std::endl;
    std::cout << std::setw(10) << time << std::setw(15) << g_cwnd << std::endl;
}

// RTT change function
static void RttChange(Time oldRtt, Time newRtt) {
    double time = Simulator::Now().GetSeconds();
    double g_rtt = newRtt.GetMilliSeconds();  // RTT in milliseconds
    rttFile << time << " " << g_rtt << std::endl;
    std::cout << std::setw(10) << "Time" << std::setw(25) << "RTT (ms)" << std::endl;
    std::cout << std::setw(10) << time << std::setw(25) << g_rtt << std::endl;
}

// Throughput calculation function
static void findThroughput() {
    Time currentTime = Simulator::Now();
    double time = currentTime.GetSeconds();
    double currentThroughput = (sink->GetTotalRx() - lastTotalRx) * 8.0 / 1e6;  // Converted to Mbps
    throughputFile << time << " " << currentThroughput << std::endl;
    std::cout << std::setw(10) << "Time" << std::setw(20) << "Throughput (Mbps)" << std::endl;
    std::cout << std::setw(10) << time << std::setw(20) << currentThroughput << std::endl;
    lastTotalRx = sink->GetTotalRx();
    Simulator::Schedule(Seconds(1.0), &findThroughput);  // Sample throughput every 1 second
}

// Function to trace Cwnd and RTT for a given socket
static void TraceCwndRtt(Ptr<Socket> socket) {
    Ptr<TcpSocketBase> tcpSocket = DynamicCast<TcpSocketBase>(socket);
    if (tcpSocket) {
        tcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndChange));
        tcpSocket->TraceConnectWithoutContext("RTT", MakeCallback(&RttChange));
        std::cout << "Traces attached to the socket" << std::endl;
    } else {
        std::cout << "Failed to attach traces. Socket is not a TcpSocketBase." << std::endl;
    }
}

// Function to extract socket from BulkSendApplication and attach traces
static void AttachSocketTraces(Ptr<Application> app) {
    Ptr<BulkSendApplication> bulkSendApp = DynamicCast<BulkSendApplication>(app);
    if (bulkSendApp) {
        Ptr<Socket> socket = bulkSendApp->GetSocket();
        if (socket) {
            TraceCwndRtt(socket);
        } else {
            std::cout << "Socket not available yet. Retrying in 0.1s." << std::endl;
            Simulator::Schedule(Seconds(0.1), &AttachSocketTraces, app);  // Retry after 0.1 seconds
        }
    } else {
        std::cout << "Application is not a BulkSendApplication." << std::endl;
    }
}

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    int tcpSegmentSize = TCP_SEGMENT_SIZE;
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(tcpSegmentSize));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(2));
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpCubic"));

    NodeContainer nodes;
    nodes.Create(NUM_NODES);  // Now 10 nodes

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(RING_DATA_RATE));
    pointToPoint.SetChannelAttribute("Delay", StringValue(RING_DELAY));

    NetDeviceContainer devices;
    InternetStackHelper stack;
    stack.Install(nodes);
    Ipv4AddressHelper address;

    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        NetDeviceContainer link;
        if (i == nodes.GetN() - 1) {
            // Connect the last node to the first node to complete the ring
            link = pointToPoint.Install(NodeContainer(nodes.Get(i), nodes.Get(0)));
        } else {
            // Connect the current node to the next node
            link = pointToPoint.Install(NodeContainer(nodes.Get(i), nodes.Get(i + 1)));
        }
        devices.Add(link);
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        address.Assign(link);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t serverPort = 9;

    Address sinkAddr(InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddr);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(NUM_NODES - 1)); // Install on the last node
    sinkApp.Start(Seconds(0.01));
    sinkApp.Stop(Seconds(DURATION));
    sink = DynamicCast<PacketSink>(sinkApp.Get(0));

    // Get the IP address of the last node
    Ptr<Ipv4> ipv4 = nodes.Get(NUM_NODES - 1)->GetObject<Ipv4>();
    Ipv4Address destAddress = ipv4->GetAddress(1, 0).GetLocal();

    // Set up BulkSendApplication as the traffic generator on the first node
    BulkSendHelper sourceHelper("ns3::TcpSocketFactory", InetSocketAddress(destAddress, serverPort));
    sourceHelper.SetAttribute("MaxBytes", UintegerValue(0));  // Send unlimited data
    ApplicationContainer sourceApp = sourceHelper.Install(nodes.Get(0));
    sourceApp.Start(Seconds(0.0));
    sourceApp.Stop(Seconds(DURATION));

    // Attach Cwnd and RTT tracers for the socket after the app starts
    Simulator::Schedule(Seconds(0.1), &AttachSocketTraces, sourceApp.Get(0));

    // Open the output files
    std::string outputDir = "/path/to/sourcens3/folder/desired/output/file/";//CHANGE THIS 
    cwndFile.open(outputDir + "tcpcubic.cwnd");
    rttFile.open(outputDir + "tcpcubic.rtt");
    throughputFile.open(outputDir + "tcpcubic.throughput");
    packetLossFile.open(outputDir + "tcpcubic.packetloss");  // New file for packet loss logging
    if (!cwndFile.is_open() || !rttFile.is_open() || !throughputFile.is_open() || !packetLossFile.is_open()) {
        std::cerr << "Error opening output files" << std::endl;
        return 1;
    }

    // Trace packet transmissions and receptions
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacTx", MakeCallback(&PacketSent));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacRx", MakeCallback(&PacketReceived));

    // Schedule throughput and packet loss calculations
    Simulator::Schedule(Seconds(0.01), &findThroughput);
    Simulator::Schedule(Seconds(1.0), &CalculatePacketLoss);

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
