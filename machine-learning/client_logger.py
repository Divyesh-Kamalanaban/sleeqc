# collect_dataset.py
import socket, time, serial, threading, csv, random
from datetime import datetime

ESP_IP = "192.168.137.200"
ESP_PORT = 8000
SERIAL_PORT = "COM7"
BAUD = 115200
LOG_CSV = "dataset_raw.csv"
SAMPLES = 10000
SAMPLE_INTERVAL = 2.0  # seconds

# message sizes to vary
MSG_SIZES = [32, 64, 128, 256, 512, 1024]

# Thread-safe placeholders
serial_lines = []
stop_event = threading.Event()

def serial_reader():
    with serial.Serial(SERIAL_PORT, BAUD, timeout=1) as ser:
        while not stop_event.is_set():
            try:
                line = ser.readline().decode('utf-8').strip()
                if line and ',' in line and line[0].isdigit():
                    serial_lines.append((time.time(), line))
            except Exception:
                pass

def get_latest_telemetry(timeout=1.0):
    # return the last telemetry line within timeout or None
    t0 = time.time()
    while time.time() - t0 < timeout:
        if serial_lines:
            return serial_lines.pop(0)[1]
        time.sleep(0.01)
    return None

def generate_message(size):
    return bytes([random.randint(0,255) for _ in range(size)])

def rule_label(free_heap_kb, sign_time_ms, stack_hwm):
    # Simple conservative rule: if plenty of heap and sign latency low -> use D5
    if free_heap_kb > 200 and sign_time_ms < 200 and stack_hwm > 4096:
        return 1  # Dilithium5
    return 0  # Dilithium2

def main():
    ser_thread = threading.Thread(target=serial_reader, daemon=True)
    ser_thread.start()
    with open(LOG_CSV, "w", newline='') as f:
        writer = csv.writer(f)
        writer.writerow(["timestamp","free_heap_kb","stack_hwm_bytes","net_activity",
                         "msg_size","sign_time_ms","label"])
        for i in range(SAMPLES):
            msg = generate_message(random.choice(MSG_SIZES))
            payload = msg  # Optionally prefix algorithm request string
            # send request and measure time
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(3)
                t0 = time.time()
                sock.connect((ESP_IP, ESP_PORT))
                sock.sendall(payload)
                sig = sock.recv(8192)
                t1 = time.time()
                sock.close()
                sign_time_ms = (t1 - t0) * 1000.0
            except Exception as e:
                print("Network error", e)
                sign_time_ms = 9999.0

            # get most recent telemetry line
            tel = get_latest_telemetry(timeout=1.0)
            if tel is None:
                # If telemetry not available, skip or record NaNs
                continue
            parts = tel.split(',')
            # expected: timestamp,free_heap_kb,stack_hwm,net_activity
            if len(parts) < 4: 
                continue
            try:
                free_heap_kb = float(parts[1])
                stack_hwm = int(parts[2])
                net_activity = int(parts[3])
            except:
                continue

            label = rule_label(free_heap_kb, sign_time_ms, stack_hwm)
            writer.writerow([int(time.time()*1000), free_heap_kb, stack_hwm, net_activity,
                             len(msg), round(sign_time_ms,2), label])
            f.flush()
            print(f"Saved sample {i+1}/{SAMPLES} label={label} heap={free_heap_kb} sign_ms={sign_time_ms:.1f}")

            time.sleep(SAMPLE_INTERVAL)

    stop_event.set()
    ser_thread.join()

if __name__ == "__main__":
    main()
