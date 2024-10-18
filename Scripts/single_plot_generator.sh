#!/bin/bash

# Define the output directory
OUTPUT_DIR="/path/to/generated/text/files/in/outputs" //CHANGE THIS ACCORDING TO WHERE YOU PUT YOUR OUTPUTS INSIDE NS3

# Define the scratch directory
SCRATCH_DIR="/home/mitsal/Downloads/ns-allinone-3.41/ns-3.41/scratch"

# Create the outputs directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

# Function to plot graph
plot_graph() {
    local DATA_FILE=$1
    local OUTPUT_FILE=$2
    local YLABEL=$3

    # Check if the data file is not empty
    if [ -s "$DATA_FILE" ]; then
        gnuplot -e "
            set terminal pngcairo enhanced font 'Times,12' size 800,600;
            set output '$OUTPUT_FILE';
            set xlabel 'Time (s)';
            set ylabel '$YLABEL';
            unset title;
            set grid;
            set key off;
            plot '$DATA_FILE' using 1:2 with lines linewidth 2;"
    else
        echo "Data file $DATA_FILE is empty or does not contain valid points."
    fi
}

# Array of file names for the simulation, including packet loss
SIMULATION_FILES=("quicbbr.cwnd" "quicbbr.rtt" "quicbbr.throughput" "quicbbr.packetloss")

# Function to run simulation
run_simulation() {
    local SIMULATION_NAME=$1
    local FILES_ARRAY=("${@:2}")

    # Check if all required files exist
    local all_files_exist=true
    for FILE in "${FILES_ARRAY[@]}"; do
        FILE_PATH="$OUTPUT_DIR/$FILE"
        if [[ ! -f "$FILE_PATH" ]] || [[ ! -s "$FILE_PATH" ]]; then
            all_files_exist=false
            break
        fi
    done

    # Run simulation only if necessary
    if [[ "$all_files_exist" == false ]]; then
        echo "Running $SIMULATION_NAME simulation"
        cd "$SCRATCH_DIR"  # Adjust path to ns-3 scratch directory
        ./waf --run "$SIMULATION_NAME"
    else
        echo "Simulation data already exists. Skipping simulation run."
    fi

    # Process files and plot graphs
    for FILE in "${FILES_ARRAY[@]}"; do
        FILE_PATH="$OUTPUT_DIR/$FILE"
        
        # Determine y-label based on file type
        local YLABEL=""
        if [[ $FILE == *"cwnd"* ]]; then
            YLABEL="Congestion Window (Packets)"
        elif [[ $FILE == *"rtt"* ]]; then
            YLABEL="Round-Trip Time (ms)"
        elif [[ $FILE == *"throughput"* ]]; then
            YLABEL="Throughput (Mbps)"
        elif [[ $FILE == *"packetloss"* ]]; then
            YLABEL="Packet Loss (%)"
        else
            echo "Unknown file type: $FILE"
            continue
        fi

        # Determine output file name based on the parameter type (e.g., quicbbr.cwnd.png)
        OUTPUT_FILE="$OUTPUT_DIR/${FILE}.png"

        # Plot graph for each file
        echo "Plotting $SIMULATION_NAME $FILE"
        plot_graph "$FILE_PATH" "$OUTPUT_FILE" "$YLABEL"
    done
}

# Run the simulation
run_simulation "quicbbr_bus_topology" "${SIMULATION_FILES[@]}" //CHANGE THE NAME AS YOU WANT FOR THE RESULTING GRAPH

echo "Complete!"   
