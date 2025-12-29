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

data_dir = Path(__file__).resolve().parent / "output"

data_pdvu = pd.read_csv(data_dir / "pdvu_img_heavy.csv")
data_tdf = pd.read_csv(data_dir / "tdf_img_heavy.csv")
data_preview = pd.read_csv(data_dir / "Preview_img_heavy.csv")

print(f"pdvu: MIN:{data_pdvu['rss_mb'].min()}, MAX:{data_pdvu['rss_mb'].max()}, Mean:{data_pdvu['rss_mb'].mean()}")
print(f"tdf: MIN:{data_tdf['rss_mb'].min()}, MAX:{data_tdf['rss_mb'].max()}, Mean:{data_tdf['rss_mb'].mean()}")
print(f"preview: MIN:{data_preview['rss_mb'].min()}, MAX:{data_preview['rss_mb'].max()}, Mean:{data_preview['rss_mb'].mean()}")

plt.figure(figsize=(10, 5), dpi=120)
plt.plot(data_pdvu['time'], data_pdvu['rss_mb'],linestyle='-', color='blue', label="pdvu")
plt.plot(data_tdf['time'], data_tdf['rss_mb'],linestyle='-', color='red', label="tdf")
plt.plot(data_preview['time'], data_preview['rss_mb'],linestyle='-', color='black', label="preview")
plt.xlabel("Time(s)")
plt.ylabel("Memory Usage (MB)")
plt.title("Stress Test Memory Usage Benchmark(Image Heavy)")
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.show()

print("################################ text pdf data ################################")

data_pdvu_t = pd.read_csv(data_dir / "pdvu_text.csv")
data_tdf_t = pd.read_csv(data_dir / "tdf_text.csv")
data_preview_t = pd.read_csv(data_dir / "Preview_text.csv")

print(f"pdvu: MIN:{data_pdvu_t['rss_mb'].min()}, MAX:{data_pdvu_t['rss_mb'].max()}, Mean:{data_pdvu_t['rss_mb'].mean()}")
print(f"tdf: MIN:{data_tdf_t['rss_mb'].min()}, MAX:{data_tdf_t['rss_mb'].max()}, Mean:{data_tdf_t['rss_mb'].mean()}")
print(f"preview: MIN:{data_preview_t['rss_mb'].min()}, MAX:{data_preview_t['rss_mb'].max()}, Mean:{data_preview_t['rss_mb'].mean()}")


plt.figure(figsize=(10, 5), dpi=120)
plt.plot(data_pdvu_t['time'], data_pdvu_t['rss_mb'],linestyle='-', color='blue', label="pdvu")
plt.plot(data_tdf_t['time'], data_tdf_t['rss_mb'],linestyle='-', color='red', label="tdf")
plt.plot(data_preview_t['time'], data_preview_t['rss_mb'],linestyle='-', color='black', label="preview")
plt.xlabel("Time(s)")
plt.ylabel("Memory Usage (MB)")
plt.title("Stress Test Memory Usage Benchmark(Text heavy)")
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.show()

print("################################ auto test text pdf data ################################")

data_pdvu_a = pd.read_csv(data_dir / "pdvu_auto_bench_text.csv")
data_tdf_a = pd.read_csv(data_dir / "tdf_auto_bench_text.csv")
data_preview_a = pd.read_csv(data_dir / "Preview_auto_bench_text.csv")

print(f"pdvu: MIN:{data_pdvu_a['rss_mb'].min()}, MAX:{data_pdvu_a['rss_mb'].max()}, Mean:{data_pdvu_a['rss_mb'].mean()}")
print(f"tdf: MIN:{data_tdf_a['rss_mb'].min()}, MAX:{data_tdf_a['rss_mb'].max()}, Mean:{data_tdf_a['rss_mb'].mean()}")
print(f"preview: MIN:{data_preview_a['rss_mb'].min()}, MAX:{data_preview_a['rss_mb'].max()}, Mean:{data_preview_a['rss_mb'].mean()}")

plt.figure(figsize=(10, 5), dpi=120)
plt.plot(data_pdvu_a['time'], data_pdvu_a['rss_mb'],linestyle='-', color='blue', label="pdvu")
plt.plot(data_tdf_a['time'], data_tdf_a['rss_mb'],linestyle='-', color='red', label="tdf")
plt.plot(data_preview_a['time'], data_preview_a['rss_mb'],linestyle='-', color='black', label="preview")
plt.xlabel("Time(s)")
plt.ylabel("Memory Usage (MB)")
plt.title("Auto Test Memory Benchmark(Text heavy)")
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.show()

print("################################ auto test img heavy pdf data ################################")

data_pdvu_img = pd.read_csv(data_dir / "pdvu_auto_bench_img_heavy.csv")
data_tdf_img = pd.read_csv(data_dir / "tdf_auto_bench_img_heavy.csv")
data_preview_img = pd.read_csv(data_dir / "preview_auto_bench_img_heavy.csv")

print(f"pdvu: MIN:{data_pdvu_img['rss_mb'].min()}, MAX:{data_pdvu_img['rss_mb'].max()}, Mean:{data_pdvu_img['rss_mb'].mean()}")
print(f"tdf: MIN:{data_tdf_img['rss_mb'].min()}, MAX:{data_tdf_img['rss_mb'].max()}, Mean:{data_tdf_img['rss_mb'].mean()}")
print(f"preview: MIN:{data_preview_img['rss_mb'].min()}, MAX:{data_preview_img['rss_mb'].max()}, Mean:{data_preview_img['rss_mb'].mean()}")

plt.figure(figsize=(10, 5), dpi=120)
plt.plot(data_pdvu_img['time'], data_pdvu_img['rss_mb'],linestyle='-', color='blue', label="pdvu")
plt.plot(data_tdf_img['time'], data_tdf_img['rss_mb'],linestyle='-', color='red', label="tdf")
plt.plot(data_preview_img['time'], data_preview_img['rss_mb'],linestyle='-', color='black', label="preview")
plt.xlabel("Time(s)")
plt.ylabel("Memory Usage (MB)")
plt.title("Auto Test Memory Benchmark(Image heavy)")
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.show()



