# QUIC-BBR vs. TCP-CUBIC Comparison over Wired Networks

This repository is designed for researchers and professionals interested in simulating network protocols using the ns-3 network simulator. The content presented here is the culmination of my dissertation, focusing on the comparative performance analysis of two prominent transport protocols:

- **QUIC Protocol**: Utilizing the BBR congestion control algorithm.
- **TCP Protocol**: Utilizing the CUBIC congestion control algorithm.

Simulations were conducted across five different wired network topologies to assess the performance of both protocols under varying conditions.

## Requirements

To replicate the simulations and analyses provided in this repository, the following software versions and system setups are required:

- **ns-3.41**: For simulations involving the QUIC protocol.
- **ns-3.35**: For simulations involving the TCP protocol.

You can download them both from the official ns-3 website: [ns-3 Releases](https://www.nsnam.org/releases/).

- **Operating System**: Ubuntu 18.04 (Bionic Beaver)

For installation details, please refer to: [Ubuntu 18.04 Downloads](https://www.releases.ubuntu.com/bionic/).

### QUIC Implementation

For implementing QUIC in ns-3, you can utilize the QUIC module developed by Signet Lab, available at their GitHub repository: [Signet Lab QUIC Module](https://github.com/signetlabdei/quic). Note that the repository may be outdated or moved; however, relevant information should still be accessible.

## Project Overview

This project aims to simulate and compare the performance of the QUIC and TCP protocols. Specifically:

- **QUIC simulations**: Utilized the BBR congestion control algorithm.
- **TCP simulations**: Utilized the CUBIC congestion control algorithm.

The simulations were conducted over five different wired network topologies:

- Point-to-Point
- Star
- Bus
- Ring
- Mesh

## Performance Metrics Calculated

During the simulations, the following key performance metrics are recorded:

- **Congestion Window (Cwnd)**: Monitors the size of the congestion window over time.
- **Round-Trip Time (RTT)**: Measures the round-trip latency between nodes.
- **Throughput**: Evaluates the rate of data transfer across the network (in Mbps).
- **Packet Loss**: Tracks the percentage of packets lost during transmission.

The results are documented through detailed graphs and tables, providing a comprehensive comparative analysis of each protocol's performance in various scenarios.
