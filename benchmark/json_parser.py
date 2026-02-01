"""
Class to parse json output from footprint cli tool into csv
"""

import json
import csv


class JsonParser:
    def __init__(self, path_to_json: str, out_path: str):
        self.path_to_json = path_to_json
        self.out_path = out_path
        try:
            with open(path_to_json, "r") as f:
                self.data = json.load(f)
        except FileNotFoundError:
            raise RuntimeError("File not found")

    def parse_and_write(self):
        data = self.parse_data()
        self.write_to_csv(data)

    def parse_data(self) -> dict:
        """
        parse a sample extracting time stamp and total footprint
        """
        footprint_data = {}  # entry, footprint_mb
        start_time = None
        for sample in self.data["samples"]:
            total_footprint_mb = sample.get("total footprint") / (1024 * 1024)
            time_stamp = sample.get("start_time").get("wall_time_s")
            if not start_time:
                start_time = time_stamp

            rel_time = time_stamp - start_time

            footprint_data[round(rel_time, 2)] = round(total_footprint_mb, 2)

        return footprint_data

    def write_to_csv(self, data: dict):
        """
        write the parsed json data into a csv file
        """
        with open(self.out_path, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["time", "physical_footprint_mb"])
            for time, physical_footprint_mb in data.items():
                writer.writerow([time, physical_footprint_mb])

        print(f"Parsed Json into CSV at output path: {self.out_path}")
