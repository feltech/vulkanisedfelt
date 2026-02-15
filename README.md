# vulkanisedfelt
Vulkan learning

## Building via nix
```
nix develop ./build_env --command cmake --build build/build/Debug
```

## Running tests via nix

```
nix develop ./build_env --command ctest --test-dir build/Debug
```

## Building and running tests via nix

```
nix develop ./build_env --command cmake --build build/Debug && nix develop ./build_env --command ctest --test-dir build/Debug
```