import zenoh
import json
import sys
import argparse
import os
import csv
from datetime import datetime

# Parse command line arguments
parser = argparse.ArgumentParser(description="Zenoh GPS Data Subscriber")
parser.add_argument('--save-csv', action='store_true', help='Save received data to gps_data.csv')
args = parser.parse_args()

# Zenoh configuration for UDP transport
conf = zenoh.Config()
conf.insert_json5('mode', '"peer"')
conf.insert_json5('listen/endpoints', '["udp/0.0.0.0:7447"]')

session = zenoh.open(conf)

def listener(sample):
    try:
        # Assuming the version uses to_bytes().decode('utf-8')
        payload_str = sample.payload.to_bytes().decode('utf-8')
        data = json.loads(payload_str)
        print("Received GPS data:")
        print(data)

        if args.save_csv:
            # Generate filename with current date and time
            current_time = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
            file = f"{current_time}_gps_data.csv"
            columns = ['utc_timestamp', 'utc', 'hr', 'min', 'sec', 'ms', 'frequency', 'rtk_solution', 'status', 'lat', 'lon', 'h', 'S_llh_h', 'S_llh_v', 'ecef_x', 'ecef_y', 'ecef_z', 'S_ecef', 'n', 'e', 'd', 'S_rtk_x_h', 'S_rtk_x_v', 'v_n', 'v_e', 'v_d', 'S_rtk_v_h', 'S_rtk_v_v', 'sats']
            row = [data.get(col, '') for col in columns]
            file_exists = os.path.exists(file) and os.path.getsize(file) > 0
            with open(file, 'a', newline='') as f:
                writer = csv.writer(f)
                if not file_exists:
                    writer.writerow([col.upper() for col in columns])  # Match C++ header style
                writer.writerow(row)
    except Exception as e:
        print(f"Error decoding payload: {e}")

sub = session.declare_subscriber('fdcl/piksi', listener)

print("Subscribed to 'fdcl/piksi'. Press Ctrl+C to exit.")
sys.stdin.readline()

sub.undeclare()
session.close()