#include "piksi_multi_gps.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <ctime>

// Function to print GPS data, extracted from the callback
void print_gps_data(const piksi::PiksiMultiGPS& gps) {
    const piksi::PiksiData& data = gps.get_data();

    // Determine status string and color based on fix mode (lower 3 bits of status)
    std::string status_str;
    std::string color;
    int fix_mode = data.status & 0x07;
    switch (fix_mode) {
        case 1: status_str = "SPP"; color = "\033[31m"; break; // Red
        case 6: status_str = "SBAS"; color = "\033[35m"; break; // Purple
        case 2: status_str = "DGPS"; color = "\033[36m"; break; // Cyan
        case 3: status_str = "RTK Float"; color = "\033[34m"; break; // Blue
        case 4: status_str = "RTK Fixed"; color = "\033[32m"; break; // Green
        case 5: status_str = "DR"; color = "\033[37m"; break; // White
        default: status_str = "Unknown/Invalid"; color = "\033[31m"; break; // Red for invalid
    }

    // Calculate latency
    std::string latency_str;
    if (data.utc_timestamp >= 0.0) {
        double now_time = static_cast<double>(std::time(nullptr));
        double latency = now_time - data.utc_timestamp;
        std::ostringstream oss;
        oss << std::setprecision(3) << latency << " seconds";
        latency_str = oss.str();
    } else {
        latency_str = "N/A";
    }

    std::cout << "\n=== GPS Data ===" << std::endl;
    std::cout << "UTC Time: " << std::setprecision(2) << data.utc << " ("
              << static_cast<int>(data.hr) << ":"
              << static_cast<int>(data.min) << ":"
              << static_cast<int>(data.sec) << "." << data.ms << ")" << std::endl;
    std::cout << "LLH: Lat=" << std::setprecision(9) << data.lat << " deg, Lon=" << data.lon
              << " deg, Height=" << data.h << " m" << std::endl;
    std::cout << "LLH Accuracy: Horizontal=" << data.S_llh_h << " m, Vertical=" << data.S_llh_v << " m" << std::endl;
    std::cout << "ECEF: X=" << data.ecef_x << " m, Y=" << data.ecef_y << " m, Z=" << data.ecef_z << " m" << std::endl;
    std::cout << "ECEF Accuracy: " << data.S_ecef << " m" << std::endl;
    std::cout << "Baseline NED: N=" << data.n << " m, E=" << data.e << " m, D=" << data.d << " m" << std::endl;
    std::cout << "RTK Accuracy: Horizontal=" << data.S_rtk_x_h << " m, Vertical=" << data.S_rtk_x_v << " m" << std::endl;
    std::cout << "Velocity NED: N=" << data.v_n << " m/s, E=" << data.v_e << " m/s, D=" << data.v_d << " m/s" << std::endl;
    std::cout << "Velocity Accuracy: Horizontal=" << data.S_rtk_v_h << " m/s, Vertical=" << data.S_rtk_v_v << " m/s" << std::endl;
    std::cout << "Satellites: " << data.sats << std::endl;
    std::cout << color << "Status: " << status_str << (data.rtk_solution ? " (RTK Solution)" : "") << "\033[0m" << std::endl;
    std::cout << "Update Frequency: " << std::setprecision(2) << data.frequency << " Hz" << std::endl;
    std::cout << "Data Transmission Latency: " << latency_str << std::endl;
}

int main() {
    piksi::PiksiMultiGPS gps("../config.cfg");

    std::time_t now = std::time(nullptr);
    std::tm* local_time = std::localtime(&now);
    std::ostringstream filename;
    filename << std::put_time(local_time, "%Y-%m-%d_%H-%M-%S") << "_gps_data.csv";

    std::ofstream csv_file(filename.str());
    if (!csv_file.is_open()) {
        std::cerr << "Failed to open gps_data.csv for logging!" << std::endl;
        return 1;
    }
    bool header_written = false;

    std::cout << "GPS: Initializing..." << std::endl;
    gps.open();
    gps.configure();
    gps.init_loop();

    while (true) {
        gps.loop();
        // Check if there is new data to print and log
        if (gps.has_new_data()) {
            const piksi::PiksiData& data = gps.get_data();
            if (gps.get_log_to_csv()) {
                if (!header_written) {
                    csv_file << "UTC_timestamp,UTC,HR,MIN,SEC,MS,Frequency,RTK_solution,Status,Lat,Lon,Height,S_llh_h,S_llh_v,ECEF_x,ECEF_y,ECEF_z,S_ecef,Baseline_n,Baseline_e,Baseline_d,S_rtk_x_h,S_rtk_x_v,Vel_n,Vel_e,Vel_d,S_rtk_v_h,S_rtk_v_v,Sats\n";
                    header_written = true;
                }
                csv_file << std::setprecision(15) << data.utc_timestamp << ","
                         << data.utc << ","
                         << static_cast<int>(data.hr) << ","
                         << static_cast<int>(data.min) << ","
                         << static_cast<int>(data.sec) << ","
                         << data.ms << ","
                         << data.frequency << ","
                         << data.rtk_solution << ","
                         << data.status << ","
                         << data.lat << ","
                         << data.lon << ","
                         << data.h << ","
                         << data.S_llh_h << ","
                         << data.S_llh_v << ","
                         << data.ecef_x << ","
                         << data.ecef_y << ","
                         << data.ecef_z << ","
                         << data.S_ecef << ","
                         << data.n << ","
                         << data.e << ","
                         << data.d << ","
                         << data.S_rtk_x_h << ","
                         << data.S_rtk_x_v << ","
                         << data.v_n << ","
                         << data.v_e << ","
                         << data.v_d << ","
                         << data.S_rtk_v_h << ","
                         << data.S_rtk_v_v << ","
                         << data.sats << "\n";
                csv_file.flush();
            }
            print_gps_data(gps);
        }
    }

    csv_file.close();
    gps.close();
    return 0;
}