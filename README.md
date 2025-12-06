## pdvu
A fast and lightweight terminal based pdf viewer

### Build instructions
#### Debug (Tracy Enabled)
```
# Configure
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build/debug --parallel

# Run
./build/debug/pvdu
```
#### Profiling with Tracy (Tracy Enabled)
```
# Configure
cmake -S . -B build/profiling -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Build (using all CPU cores)
cmake --build build/profiling --parallel

# Run
./build/profiling/pdvu
```
#### Release (Tracy disabled)
```
# Configure
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build/release --parallel

# Run
./build/release/pdvu
```