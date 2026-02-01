# /// script
# requires-python = ">=3.12"
# dependencies = [
#     "pandas",
#     "matplotlib"
# ]
# ///

import pandas as pd
from matplotlib import pyplot as plt
from pathlib import Path


data_dir = Path(__file__).resolve().parent / "output" / "csv"

print("################################ text pdf data(Physical Footprint) ################################")
data_pdvu = pd.read_csv(data_dir / "pdvu_footprint_auto_bench_text_heavy.csv")
data_tdf = pd.read_csv(data_dir / "tdf_footprint_auto_bench_text_heavy.csv")
data_preview = pd.read_csv(data_dir / "Preview_footprint_auto_bench_text_heavy.csv")

print(f"pdvu: MIN:{data_pdvu['physical_footprint_mb'].min()}, MAX:{data_pdvu['physical_footprint_mb'].max()}, Mean:{data_pdvu['physical_footprint_mb'].mean()}")
print(f"tdf: MIN:{data_tdf['physical_footprint_mb'].min()}, MAX:{data_tdf['physical_footprint_mb'].max()}, Mean:{data_tdf['physical_footprint_mb'].mean()}")
print(f"preview: MIN:{data_preview['physical_footprint_mb'].min()}, MAX:{data_preview['physical_footprint_mb'].max()}, Mean:{data_preview['physical_footprint_mb'].mean()}")

plt.figure(figsize=(10, 5), dpi=120)
plt.plot(data_pdvu['time'], data_pdvu['physical_footprint_mb'],linestyle='-', color='blue', label="pdvu")
plt.plot(data_tdf['time'], data_tdf['physical_footprint_mb'],linestyle='-', color='red', label="tdf")
plt.plot(data_preview['time'], data_preview['physical_footprint_mb'],linestyle='-', color='black', label="preview")
plt.xlabel("Time(s)")
plt.ylabel("physical_footprint(MB)")
plt.title("Auto Test Memory Usage Benchmark(Text Heavy)")
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.show()

print("################################ image heavy pdf data(Physical Footprint) ################################")
data_pdvu = pd.read_csv(data_dir / "pdvu_footprint_auto_bench_img_heavy.csv")
data_tdf = pd.read_csv(data_dir / "tdf_footprint_auto_bench_img_heavy.csv")
data_preview = pd.read_csv(data_dir / "Preview_footprint_auto_bench_img_heavy.csv")

print(f"pdvu: MIN:{data_pdvu['physical_footprint_mb'].min()}, MAX:{data_pdvu['physical_footprint_mb'].max()}, Mean:{data_pdvu['physical_footprint_mb'].mean()}")
print(f"tdf: MIN:{data_tdf['physical_footprint_mb'].min()}, MAX:{data_tdf['physical_footprint_mb'].max()}, Mean:{data_tdf['physical_footprint_mb'].mean()}")
print(f"preview: MIN:{data_preview['physical_footprint_mb'].min()}, MAX:{data_preview['physical_footprint_mb'].max()}, Mean:{data_preview['physical_footprint_mb'].mean()}")

plt.figure(figsize=(10, 5), dpi=120)
plt.plot(data_pdvu['time'], data_pdvu['physical_footprint_mb'],linestyle='-', color='blue', label="pdvu")
plt.plot(data_tdf['time'], data_tdf['physical_footprint_mb'],linestyle='-', color='red', label="tdf")
plt.plot(data_preview['time'], data_preview['physical_footprint_mb'],linestyle='-', color='black', label="preview")
plt.xlabel("Time(s)")
plt.ylabel("physical_footprint(MB)")
plt.title("Auto Test Memory Usage Benchmark(Image Heavy)")
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.show()
