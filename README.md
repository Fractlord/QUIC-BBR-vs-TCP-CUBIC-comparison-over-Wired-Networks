# QUIC-BBR-vs-TCP-CUBIC-comparison-over-Wired-Networks

This repository is designed for researchers and professionals interested in simulating network protocols using the ns-3 network simulator. The content presented here is the result of my dissertation, which focuses on the comparative performance analysis of two prominent transport protocols:

QUIC protocol, utilizing the BBR congestion control algorithm
TCP protocol, utilizing the CUBIC congestion control algorithm

The simulations were conducted across five different wired network topologies to assess the performance of both protocols under varying conditions.

REQUIREMENTS 

To replicate the simulations and analysis provided in this repository, the following software versions and system setup were used:

ns-3.41 for simulations involving the QUIC protocol.
ns-3.35 for simulations involving the TCP protocol.

You can download them both from the official ns-3 website ->> https://www.nsnam.org/releases/

Ubuntu 18.04 (Bionic Beaver) as the operating system for running the simulations.

More details on installing Ubuntu 18.04 can be found  here --->>>> https://www.releases.ubuntu.com/bionic/

For implementing QUIC in ns-3, you can utilize the QUIC module developed by Signet Lab, which is available on their GitHub repository -->>https://github.com/signetlabdei/quic
Note: The repository may be outdated or moved to a different source, but the relevant information should still be available, and they will provide guidance or redirect you accordingly.

PROJECT OVERVIEW

This project is aimed at simulating and comparing the performance of the QUIC and TCP protocols. Specifically:

QUIC simulations utilized the BBR congestion control algorithm.
TCP simulations utilized the CUBIC congestion control algorithm.
The simulations were conducted over five different WIRED network topologies, including:

Point-to-point topology
Star topology
Bus topology
Ring topology
Mesh topology

The results of these simulations are documented in the form of graphs and tables, providing a clear comparative analysis of throughput, latency, congestion window behavior, and packet loss.
