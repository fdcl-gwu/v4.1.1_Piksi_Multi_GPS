#include "piksi_multi_gps.hpp"
#include <libsbp/legacy/api.h>
#include <libsbp/legacy/compat.h>
#include <libsbp/legacy/cpp/payload_handler.h>
#include <thread>
#include <iostream>
#include <ctime>
#include <cstdlib> // for setenv
#include <iomanip>
#include <sstream>
#include <fstream>
#include <cstring>
#include <zenoh.hxx>

using namespace zenoh;

namespace piksi {

static struct sp_port *piksi_port;
int PiksiMultiGPS::loop_count_ = 0;
bool PiksiMultiGPS::flag_start_ = false;

static sbp_msg_callbacks_node_t heartbeat_node_0;
static sbp_msg_callbacks_node_t gps_time_node;
static sbp_msg_callbacks_node_t pos_llh_node;
static sbp_msg_callbacks_node_t pos_ecef_node;
static sbp_msg_callbacks_node_t vel_ned_node;
static sbp_msg_callbacks_node_t baseline_node;
static sbp_msg_callbacks_node_t heartbeat_node;

PiksiMultiGPS::PiksiMultiGPS(const std::string& config_file_path)
    : serial_port_name_(nullptr) {
    // Set default values
    port_ = "/dev/cu.usbserial-AL00KUE3";
    baud_rate_ = 115200;

    read_config(config_file_path);

    data_.rtk_solution = false;
    data_.frequency = 0.0;
    data_.utc_timestamp = -1.0;
    last_update_ = std::chrono::high_resolution_clock::time_point{};

    // Initialize Zenoh session for UDP publishing
    Config zenoh_config = Config::create_default();
    zenoh_config.insert_json5("mode", "\"peer\"");
    zenoh_config.insert_json5("listen/endpoints", "[\"udp/0.0.0.0:7447\"]");
    session_ = Session::open(std::move(zenoh_config));
    pub_ = session_->declare_publisher("fdcl/piksi");
    std::cout << "Zenoh: Initialized publisher on key 'fdcl/piksi' via UDP." << std::endl;
}

PiksiMultiGPS::~PiksiMultiGPS() {
    close();
}

void PiksiMultiGPS::read_config(const std::string& config_file_path) {
    std::ifstream config_file(config_file_path);
    if (!config_file.is_open()) {
        std::cerr << "GPS: Warning - Failed to open config file. Using default settings." << std::endl;
        return;
    }

    std::string line;
    bool in_piksi_section = false;
    while (std::getline(config_file, line)) {
        // Trim whitespace from the line
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

        // Check for section header
        if (line.length() > 0 && line.front() == '[' && line.back() == ']') {
            std::string section = line.substr(1, line.length() - 2);
            in_piksi_section = (section == "Piksi Multi GPS");
            continue;
        }

        if (!in_piksi_section || line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream is_line(line);
        std::string key;
        if (std::getline(is_line, key, '=')) {
            std::string value;
            if (std::getline(is_line, value)) {
                // Trim whitespace from key and value
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                if (key == "port") {
                    port_ = value;
                } else if (key == "baud_rate") {
                    try {
                        baud_rate_ = std::stoi(value);
                    } catch (const std::invalid_argument& ia) {
                        std::cerr << "GPS: Warning - Invalid baud_rate in config. Using default." << std::endl;
                    }
                } else if (key == "log_to_csv") {
                    log_to_csv_ = (value == "true");
                }
            }
        }
    }
    config_file.close();
}

void PiksiMultiGPS::open() {
    serial_port_name_ = port_.c_str();
    std::cout << "GPS: Attempting to open " << serial_port_name_ << " with baud rate " << baud_rate_ << " .." << std::endl;

    if (!serial_port_name_) {
        std::cerr << "GPS: Check the serial port path of the Piksi!" << std::endl;
        exit(EXIT_FAILURE);
    }

    int result = sp_get_port_by_name(serial_port_name_, &piksi_port);
    if (result != SP_OK) {
        std::cerr << "GPS: Cannot find provided serial port!" << std::endl;
        exit(EXIT_FAILURE);
    }

    result = sp_open(piksi_port, SP_MODE_READ_WRITE);
    if (result != SP_OK) {
        std::cerr << "GPS: Cannot open " << serial_port_name_ << " for reading/writing!" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "GPS: Port is open" << std::endl;

    setup_port(baud_rate_);
}

void PiksiMultiGPS::configure() {
    std::cout << "GPS: Configuring solution frequency to 10 Hz..." << std::endl;

    char setting[] = "solution\0soln_freq\0";
    char value_str[] = "10\0";

    u8 payload[sizeof(setting) + sizeof(value_str)];
    memcpy(payload, setting, sizeof(setting));
    memcpy(payload + sizeof(setting), value_str, sizeof(value_str));
    u8 len = sizeof(setting) + sizeof(value_str);

    s8 ret = sbp_send_message(&s_, SBP_MSG_SETTINGS_WRITE, 0, len, payload, &piksi_port_write);
    if (ret != SBP_OK) {
        std::cerr << "GPS: Failed to send settings write message: " << static_cast<int>(ret) << std::endl;
    } else {
        std::cout << "GPS: Configuration sent successfully." << std::endl;
    }
}

void PiksiMultiGPS::setup_port(int baud) {
    std::cout << "GPS: Attempting to configure the serial port..." << std::endl;
    int result;

    result = sp_set_baudrate(piksi_port, baud);
    if (result != SP_OK) {
        std::cerr << "GPS: Cannot set port baud rate!" << std::endl;
        exit(EXIT_FAILURE);
    }

    result = sp_set_flowcontrol(piksi_port, SP_FLOWCONTROL_NONE);
    if (result != SP_OK) {
        std::cerr << "GPS: Cannot set flow control!" << std::endl;
        exit(EXIT_FAILURE);
    }

    result = sp_set_bits(piksi_port, 8);
    if (result != SP_OK) {
        std::cerr << "GPS: Cannot set data bits!" << std::endl;
        exit(EXIT_FAILURE);
    }

    result = sp_set_parity(piksi_port, SP_PARITY_NONE);
    if (result != SP_OK) {
        std::cerr << "GPS: Cannot set parity!" << std::endl;
        exit(EXIT_FAILURE);
    }

    result = sp_set_stopbits(piksi_port, 1);
    if (result != SP_OK) {
        std::cerr << "GPS: Cannot set stop bits!" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "GPS: Configuring serial port completed." << std::endl;
}

void PiksiMultiGPS::init_loop() {
    sbp_state_init(&s0_);
    sbp_register_callback(&s0_, SBP_MSG_HEARTBEAT, &heartbeat_callback_0, NULL, &heartbeat_node_0);

    sbp_state_init(&s_);
    sbp_state_set_io_context(&s_, this);
    sbp_register_callback(&s_, SBP_MSG_UTC_TIME, &gps_time_callback, this, &gps_time_node);
    sbp_register_callback(&s_, SBP_MSG_POS_ECEF, &pos_ecef_callback, this, &pos_ecef_node);
    sbp_register_callback(&s_, SBP_MSG_POS_LLH, &pos_llh_callback, this, &pos_llh_node);
    sbp_register_callback(&s_, SBP_MSG_VEL_NED, &vel_ned_callback, this, &vel_ned_node);
    sbp_register_callback(&s_, SBP_MSG_BASELINE_NED, &baseline_callback, this, &baseline_node);
    sbp_register_callback(&s_, SBP_MSG_HEARTBEAT, &heartbeat_callback, this, &heartbeat_node);

    std::cout << "\nGPS: Waiting for the first heartbeat..." << std::endl;
    while (!flag_start_) {
        sbp_process(&s0_, &piksi_port_read);
    }
    std::cout << "GPS: Starting the main loop..." << std::endl;
}

void PiksiMultiGPS::loop() {
    int ret;
    do {
        ret = sbp_process(&s_, &piksi_port_read);
        if (ret < 0) {
            std::cout << "GPS: sbp_process error: " << ret << std::endl;
        }
    } while (ret > 0);
}

void PiksiMultiGPS::close() {
    if (piksi_port) {
        int result = sp_close(piksi_port);
        if (result != SP_OK) {
            std::cerr << "GPS: Cannot close " << serial_port_name_ << " properly!" << std::endl;
        } else {
            std::cout << "GPS: Serial at " << serial_port_name_ << " port closed." << std::endl;
        }
        sp_free_port(piksi_port);
        piksi_port = nullptr;
    }
}

s32 PiksiMultiGPS::piksi_port_read(u8 *buff, u32 n, void *context) {
    (void)context;
    s32 result;
    result = sp_blocking_read(piksi_port, buff, n, 0);
    if (result < 0) {
        return SBP_READ_ERROR;
    }
    return static_cast<s32>(result);
}

s32 PiksiMultiGPS::piksi_port_write(u8 *buff, u32 n, void *context) {
    (void)context;
    s32 result = sp_blocking_write(piksi_port, buff, n, 1000);
    if (result < 0) {
        return SBP_WRITE_ERROR;
    }
    return static_cast<s32>(result);
}

void PiksiMultiGPS::heartbeat_callback_0(u16 sender_id, u8 len, u8 msg[], void *context) {
    (void)sender_id, (void)len, (void)msg, (void)context;
    std::cout << "GPS: first heartbeat detected" << std::endl;
    flag_start_ = true;
}

void PiksiMultiGPS::heartbeat_callback(u16 sender_id, u8 len, u8 msg[], void *context) {
    (void)sender_id, (void)len, (void)msg, (void)context;
}

void PiksiMultiGPS::baseline_callback(u16 sender_id, u8 len, u8 msg[], void *context) {
    (void)sender_id, (void)len, (void)context;
    msg_baseline_ned_t baseline = *(msg_baseline_ned_t *)msg;
    PiksiMultiGPS* gps = static_cast<PiksiMultiGPS*>(context);
    gps->data_.n = static_cast<float>(baseline.n) / 1.0e3;
    gps->data_.e = static_cast<float>(baseline.e) / 1.0e3;
    gps->data_.d = static_cast<float>(baseline.d) / 1.0e3;

    if (baseline.flags == 0) {
        gps->data_.rtk_solution = false;
        return;
    }

    gps->loop_count_ = 0;
    gps->data_.rtk_solution = true;
    gps->data_.status = static_cast<int>(baseline.flags);
    gps->data_.S_rtk_x_h = static_cast<float>(baseline.h_accuracy) / 1.0e3;
    gps->data_.S_rtk_x_v = static_cast<float>(baseline.v_accuracy) / 1.0e3;
}

void PiksiMultiGPS::pos_llh_callback(u16 sender_id, u8 len, u8 msg[], void *context) {
    (void)sender_id, (void)len, (void)context;
    msg_pos_llh_t pos_llh = *(msg_pos_llh_t *)msg;
    PiksiMultiGPS* gps = static_cast<PiksiMultiGPS*>(context);
    gps->data_.lat = pos_llh.lat;
    gps->data_.lon = pos_llh.lon;
    gps->data_.h = pos_llh.height;
    gps->data_.S_llh_h = static_cast<float>(pos_llh.h_accuracy) / 1.0e3;
    gps->data_.S_llh_v = static_cast<float>(pos_llh.v_accuracy) / 1.0e3;
    gps->data_.sats = static_cast<int>(pos_llh.n_sats);
    gps->loop_count_++;
    if (gps->loop_count_ > 3) {
        gps->data_.rtk_solution = false;
    }
    if (!gps->data_.rtk_solution) {
        gps->data_.status = static_cast<int>(pos_llh.flags);
    }

    // Calculate frequency based on pos_llh updates (main position message)
    auto now = std::chrono::high_resolution_clock::now();
    if (gps->last_update_.time_since_epoch().count() != 0) {
        std::chrono::duration<double> diff = now - gps->last_update_;
        double dt = diff.count();
        if (dt > 0.0) {
            gps->data_.frequency = 1.0 / dt;
        }
    }
    gps->last_update_ = now;
    
    // Set the flag to indicate new data is available
    gps->has_new_data_ = true;

    // Publish data via Zenoh as JSON
    std::ostringstream json;
    json << std::setprecision(15) << std::boolalpha;
    json << "{"
         << "\"utc_timestamp\":" << gps->data_.utc_timestamp << ","
         << "\"utc\":" << gps->data_.utc << ","
         << "\"hr\":" << static_cast<int>(gps->data_.hr) << ","
         << "\"min\":" << static_cast<int>(gps->data_.min) << ","
         << "\"sec\":" << static_cast<int>(gps->data_.sec) << ","
         << "\"ms\":" << gps->data_.ms << ","
         << "\"frequency\":" << gps->data_.frequency << ","
         << "\"rtk_solution\":" << gps->data_.rtk_solution << ","
         << "\"status\":" << gps->data_.status << ","
         << "\"lat\":" << gps->data_.lat << ","
         << "\"lon\":" << gps->data_.lon << ","
         << "\"h\":" << gps->data_.h << ","
         << "\"S_llh_h\":" << gps->data_.S_llh_h << ","
         << "\"S_llh_v\":" << gps->data_.S_llh_v << ","
         << "\"ecef_x\":" << gps->data_.ecef_x << ","
         << "\"ecef_y\":" << gps->data_.ecef_y << ","
         << "\"ecef_z\":" << gps->data_.ecef_z << ","
         << "\"S_ecef\":" << gps->data_.S_ecef << ","
         << "\"n\":" << gps->data_.n << ","
         << "\"e\":" << gps->data_.e << ","
         << "\"d\":" << gps->data_.d << ","
         << "\"S_rtk_x_h\":" << gps->data_.S_rtk_x_h << ","
         << "\"S_rtk_x_v\":" << gps->data_.S_rtk_x_v << ","
         << "\"v_n\":" << gps->data_.v_n << ","
         << "\"v_e\":" << gps->data_.v_e << ","
         << "\"v_d\":" << gps->data_.v_d << ","
         << "\"S_rtk_v_h\":" << gps->data_.S_rtk_v_h << ","
         << "\"S_rtk_v_v\":" << gps->data_.S_rtk_v_v << ","
         << "\"sats\":" << gps->data_.sats
         << "}";
    gps->pub_->put(json.str());
}

void PiksiMultiGPS::pos_ecef_callback(u16 sender_id, u8 len, u8 msg[], void *context) {
    (void)sender_id, (void)len, (void)context;
    msg_pos_ecef_t pos_ecef = *(msg_pos_ecef_t *)msg;
    PiksiMultiGPS* gps = static_cast<PiksiMultiGPS*>(context);
    gps->data_.ecef_x = pos_ecef.x;
    gps->data_.ecef_y = pos_ecef.y;
    gps->data_.ecef_z = pos_ecef.z;
    gps->data_.S_ecef = static_cast<float>(pos_ecef.accuracy) / 1.0e3;
}

void PiksiMultiGPS::vel_ned_callback(u16 sender_id, u8 len, u8 msg[], void *context) {
    (void)sender_id, (void)len, (void)context;
    msg_vel_ned_t vel_ned = *(msg_vel_ned_t *)msg;
    PiksiMultiGPS* gps = static_cast<PiksiMultiGPS*>(context);
    gps->data_.v_n = static_cast<float>(vel_ned.n) / 1.0e3;
    gps->data_.v_e = static_cast<float>(vel_ned.e) / 1.0e3;
    gps->data_.v_d = static_cast<float>(vel_ned.d) / 1.0e3;
    gps->data_.S_rtk_v_h = static_cast<float>(vel_ned.h_accuracy) / 1.0e3;
    gps->data_.S_rtk_v_v = static_cast<float>(vel_ned.v_accuracy) / 1.0e3;
}

void PiksiMultiGPS::gps_time_callback(u16 sender_id, u8 len, u8 msg[], void *context) {
    (void)sender_id, (void)len, (void)context;
    msg_utc_time_t gps_time = *(msg_utc_time_t *)msg;
    PiksiMultiGPS* gps = static_cast<PiksiMultiGPS*>(context);

    if ((gps_time.flags & 0x08) == 0) { // UTC invalid
        return; // Don't update if invalid
    }

    gps->data_.hr = gps_time.hours;
    gps->data_.min = gps_time.minutes;
    gps->data_.sec = gps_time.seconds;
    gps->data_.ms = gps_time.ns / 1.0e6;
    gps->data_.utc = gps->data_.hr + gps->data_.min / 60.0 + (gps->data_.sec + gps->data_.ms / 1000.0) / 3600.0;

    // Calculate UNIX timestamp from UTC for latency
    setenv("TZ", "UTC", 1);
    tzset();

    std::tm time_in = {};
    time_in.tm_year = gps_time.year - 1900;
    time_in.tm_mon = gps_time.month - 1;
    time_in.tm_mday = gps_time.day;
    time_in.tm_hour = gps_time.hours;
    time_in.tm_min = gps_time.minutes;
    time_in.tm_sec = gps_time.seconds;
    time_in.tm_isdst = -1;
    time_t utc_t = mktime(&time_in);
    if (utc_t != -1) {
        gps->data_.utc_timestamp = static_cast<double>(utc_t) + gps_time.ns / 1e9;
    } else {
        gps->data_.utc_timestamp = -1.0;
    }
}

} // namespace piksi