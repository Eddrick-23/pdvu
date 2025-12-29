# /// script
# requires-python = ">=3.12"
# dependencies = [
#     "psutil",
#     "matplotlib",
# ]
# ///

import psutil
import time
import argparse
import csv
from pathlib import Path
parser = argparse.ArgumentParser(
    description="Log memory usage of a process to a csv file"
)
parser.add_argument("pid", type=int, help="Process ID to monitor")
parser.add_argument("--interval", type = float, default = 0.5,  help= "Read interval in seconds")
parser.add_argument("--filename", type=str, default="", help = "Output csv name. Optional. Defaults to the pid")

args = parser.parse_args()

p = None
if (psutil.pid_exists(args.pid)):
    p = psutil.Process(args.pid)
else:
    raise RuntimeError(f"Pid: {args.pid} not found")

current_file_path = Path(__file__).resolve().parent
filename = args.filename
output_path = current_file_path / "output" / f"{filename if filename != "" else args.pid}.csv"
output_path.parent.mkdir(parents=True, exist_ok=True)

with open(output_path, "w", newline="") as f:
    count = 1
    writer = csv.writer(f)
    rss = p.memory_info().rss / (1024 * 1024)
    writer.writerow(["time", "rss_mb"])
    start = time.time()
    timestamp = time.time() - start
    writer.writerow([timestamp, rss])
    while (True):
        written = False
        try :
            rss = p.memory_info().rss / (1024 * 1024)
            timestamp = time.time() - start
            writer.writerow([timestamp, rss])
            written = True 
            time.sleep(args.interval)
        except psutil.NoSuchProcess:
            #finish writes to csv file
            if (not written):
                rss = p.memory_info().rss / (1024 * 1024)
                timestamp = time.time() - start
                writer.writerow([timestamp, rss])
            print("Process no longer exists")
            break
        except KeyboardInterrupt:
            #finish writes to the csv file
            if (not written):
                rss = p.memory_info().rss / (1024 * 1024)
                timestamp = time.time() - start
                writer.writerow([timestamp, rss])
            print(f"Tracking stopped. Lines written {count}")
            break
        count += 1
