#ifndef PIKSI_MULTI_GPS_HPP
#define PIKSI_MULTI_GPS_HPP

#include <libserialport.h>
#include <libsbp/sbp.h>
#include <libsbp/legacy/system.h>
#include <libsbp/legacy/navigation.h>
#include <libsbp/legacy/settings.h>
#include <libsbp/legacy/imu.h>
#include <libsbp/legacy/api.h>
#include <libsbp/legacy/compat.h>
#include <libsbp/legacy/cpp/payload_handler.h>
#include <string>
#include <iostream>
#include <chrono>
#include <ctime>
#include <zenoh.hxx>
#include <optional>

using namespace zenoh;

namespace piksi {

struct PiksiData {
    double lat, lon, h;              // Latitude, longitude, height (LLH)
    float S_llh_h, S_llh_v;          // LLH horizontal/vertical accuracy (m)
    double ecef_x, ecef_y, ecef_z;   // ECEF position (m)
    float S_ecef;                    // ECEF accuracy (m)
    double n, e, d;                  // Baseline NED (m)
    float S_rtk_x_h, S_rtk_x_v;      // RTK horizontal/vertical accuracy (m)
    double v_n, v_e, v_d;            // Velocity NED (m/s)
    float S_rtk_v_h, S_rtk_v_v;      // Velocity horizontal/vertical accuracy (m/s)
    u8 hr, min, sec;                 // UTC time
    double ms;                       // UTC milliseconds
    float utc;                       // UTC time in hours (hr + min/60 + sec/3600)
    double utc_timestamp;            // UNIX timestamp from UTC (seconds since epoch)
    int sats;                        // Number of satellites
    int status;                      // GPS status
    bool rtk_solution;               // RTK solution availability
    double frequency;                // Update frequency (Hz)
};

class PiksiMultiGPS {
public:
    PiksiMultiGPS(const std::string& config_file_path = "../config.cfg");
    ~PiksiMultiGPS();

    void open();
    void configure();
    void init_loop();
    void loop();
    void close();
    const PiksiData& get_data() const { return data_; }
    bool has_new_data() {
        bool temp = has_new_data_;
        has_new_data_ = false; // Reset the flag after checking
        return temp;
    }
    bool get_log_to_csv() const { return log_to_csv_; }

private:
    std::string port_;
    int baud_rate_;
    const char* serial_port_name_;
    sbp_state_t s_;
    sbp_state_t s0_;
    PiksiData data_;
    static int loop_count_;
    static bool flag_start_;
    std::chrono::time_point<std::chrono::high_resolution_clock> last_update_;
    bool has_new_data_ = false;
    std::optional<Session> session_;
    std::optional<Publisher> pub_;
    bool log_to_csv_ = true;

    void setup_port(int baud);
    void read_config(const std::string& config_file_path);
    static s32 piksi_port_read(u8 *buff, u32 n, void *context);
    static s32 piksi_port_write(u8 *buff, u32 n, void *context);

    // Callback functions for SBP messages
    static void heartbeat_callback_0(u16 sender_id, u8 len, u8 msg[], void *context);
    static void heartbeat_callback(u16 sender_id, u8 len, u8 msg[], void *context);
    static void baseline_callback(u16 sender_id, u8 len, u8 msg[], void *context);
    static void pos_llh_callback(u16 sender_id, u8 len, u8 msg[], void *context);
    static void pos_ecef_callback(u16 sender_id, u8 len, u8 msg[], void *context);
    static void vel_ned_callback(u16 sender_id, u8 len, u8 msg[], void *context);
    static void gps_time_callback(u16 sender_id, u8 len, u8 msg[], void *context);
};

} // namespace piksi

#endif