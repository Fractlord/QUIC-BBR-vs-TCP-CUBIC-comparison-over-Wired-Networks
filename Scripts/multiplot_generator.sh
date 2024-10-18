#!/bin/bash

# Set absolute file paths for all metrics
CUBIC_THROUGHPUT="path/to/1stTopology/COMPARISON/TCPCUBIC/tcpcubic.throughput"
QUIC_THROUGHPUT="path/to/1stTopology/COMPARISON/Quic/quicbbr.throughput"

CUBIC_CWND="path/to/1stTopology/COMPARISON/TCPCUBIC/tcpcubic.cwnd"
QUIC_CWND="path/to/1stTopology/COMPARISON/Quic/quicbbr.cwnd"

CUBIC_RTT="path/to/1stTopology/COMPARISON/TCPCUBIC/tcpcubic.rtt"
QUIC_RTT="path/to/1stTopology/COMPARISON/Quic/quicbbr.rtt"

CUBIC_PACKETLOSS="path/to/1stTopology/COMPARISON/TCPCUBIC/tcpcubic.packetloss"
QUIC_PACKETLOSS="path/to/1stTopology/COMPARISON/Quic/quicbbr.packetloss"



# Check if all files exist
FILES=("$CUBIC_THROUGHPUT" "$QUIC_THROUGHPUT" "$CUBIC_CWND" "$QUIC_CWND" "$CUBIC_RTT" "$QUIC_RTT" "$CUBIC_PACKETLOSS" "$QUIC_PACKETLOSS")
for FILE in "${FILES[@]}"; do
    if [[ ! -f $FILE ]]; then
        echo "Error: File not found at $FILE"
        exit 1
    fi
done

# Create a temporary gnuplot script to plot the data
GNUPLOT_SCRIPT=$(mktemp /tmp/plot_script.XXXXXX.gp)

cat << EOF > $GNUPLOT_SCRIPT
# Set the terminal to PNG with size and font
set terminal pngcairo size 1600,1200 enhanced font 'Verdana,12'

# Output file for the graph
set output 'metrics_comparison.png'

# Set multiplot layout: 2 rows, 2 columns, with some spacing
set multiplot layout 2,2 title 'TCP CUBIC vs QUIC BBR Metrics Comparison (Point-To-Point)' font 'Verdana,16'

# Common settings for all plots
set xlabel 'Time (seconds)' font 'Verdana,12'
set grid xtics ytics lc rgb "#e0e0e0" lw 1  # Light gray grid lines
set mxtics 5
set mytics 5

# Define consistent line styles with thinner lines and same line type
set style line 1 lc rgb '#FFA500' lw 1 lt 1  # TCP CUBIC: Orange, thinner line, solid
set style line 2 lc rgb '#FF4500' lw 1 lt 1  # QUIC BBR: Orange-Red, thinner line, solid

# Plot 1: Throughput
set ylabel 'Throughput (Mbps)' font 'Verdana,12'
unset logscale y  # Ensure linear scale
set yrange [0:6]  # Increased y-axis range for prominence
set title 'Throughput Comparison' font 'Verdana,14'
plot '$CUBIC_THROUGHPUT' using 1:2 with lines linestyle 1 title 'TCP CUBIC', \
     '$QUIC_THROUGHPUT' using 1:2 with lines linestyle 2 title 'QUIC BBR'

unset ylabel
unset title

# Plot 2: Congestion Window (cwnd)
set ylabel 'Congestion Window (packets)' font 'Verdana,12'
set yrange [0:80]  # Realistic cwnd values
set title 'Congestion Window (cwnd)' font 'Verdana,14'
plot '$CUBIC_CWND' using 1:2 with lines linestyle 1 title 'TCP CUBIC', \
     '$QUIC_CWND' using 1:2 with lines linestyle 2 title 'QUIC BBR'

unset ylabel
unset title

# Plot 3: Round-Trip Time (RTT)
set ylabel 'RTT (ms)' font 'Verdana,12'
set yrange [0:250]  # Adjust based on RTT values
set title 'Round-Trip Time (RTT)' font 'Verdana,14'
plot '$CUBIC_RTT' using 1:2 with lines linestyle 1 title 'TCP CUBIC', \
     '$QUIC_RTT' using 1:2 with lines linestyle 2 title 'QUIC BBR'

unset ylabel
unset title

# Plot 4: Packet Loss
set ylabel 'Packet Loss (%)' font 'Verdana,12'
set yrange [0:100]  # Adjust based on realistic packet loss percentages
set title 'Packet Loss' font 'Verdana,14'
plot '$CUBIC_PACKETLOSS' using 1:2 with lines linestyle 1 title 'TCP CUBIC', \
     '$QUIC_PACKETLOSS' using 1:2 with lines linestyle 2 title 'QUIC BBR'

unset ylabel
unset title

# End multiplot
unset multiplot
EOF

# Run the gnuplot script to generate the plot
gnuplot $GNUPLOT_SCRIPT

# Clean up the temporary gnuplot script
rm $GNUPLOT_SCRIPT

echo "Metrics comparison plot generated as metrics_comparison.png"  #CHANGE THE NAME AS YOU LIKE
