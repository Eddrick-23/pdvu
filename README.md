## pdvu
A fast and lightweight terminal based pdf viewer

### Build instructions
Optional Build Flags (Off by default)
- __DENABLE_TRACY__ Enable tracy profiling
- __DBUILD_TESTING__ Build unit tests
#### Debug
```
# Configure
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build/debug

# Run
./build/debug/pvdu <path to pdf>
```
#### Profiling with Tracy
```
# Configure
cmake -S . -B build/profiling -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_TRACY=ON

# Build (using all CPU cores)
cmake --build build/profiling

# Run
./build/profiling/pdvu <path to pdf>
```
#### Release
```
# Configure
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build/release

# Run
./build/release/pdvu <path to pdf>
```

### Unit Tests
