# Network Simulation Plot Scripts

This directory includes two **Bash scripts** using **Gnuplot** to generate visualizations for **QUIC BBR** and **TCP CUBIC** metrics across various topologies.

## Dependencies

Ensure the following is installed:
- **Gnuplot**: Install via:
 
  sudo apt install gnuplot

Single Plot Generator (single_plot_generator.sh)
Generates individual PNG plots for RTT, Throughput, Congestion Window (cwnd), and Packet Loss for a specific topology.

Required files:
quicbbr.cwnd, quicbbr.rtt, quicbbr.throughput, quicbbr.packetloss  //OR ANY OTHER NAME THAT SUITS YOU 


Multiplot Generator (multiplot_generator.sh)

Creates a 2x2 grid of plots comparing TCP CUBIC and QUIC BBR across cwnd, RTT, throughput, and packet loss.

Required files:
tcpcubic.cwnd, tcpcubic.rtt, tcpcubic.throughput, tcpcubic.packetloss
quicbbr.cwnd, quicbbr.rtt, quicbbr.throughput, quicbbr.packetloss //OR ANY OTHER NAME THAT SUITS YOU

USAGE**

--Single Plot Generator
After you edit paths in single_plot_generator.sh

Run:

./single_plot_generator.sh

--Multiplot Generator

After you edit paths in multiplot_generator.sh

Run:
./multiplot_generator.sh

CUSTOMIZING PATHS**

Edit file paths in both scripts to match your directory:
For example:
/path/to/your/data/files/

Example Directory

/path/to/topology/

├── COMPARISON/
│   ├── TCPCUBIC/
│   │   ├── tcpcubic.cwnd
│   │   ├── tcpcubic.rtt
│   │   ├── tcpcubic.throughput
│   ├── QUIC/
│   │   ├── quicbbr.cwnd
│   │   ├── quicbbr.rtt
│   │   ├── quicbbr.throughput
