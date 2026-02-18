default: run

configure:
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

configure-sanitize:
    cmake -S . -B build-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DINTRINSIC_ENABLE_SANITIZERS=ON

build: configure
    cmake --build build

test: configure
    cmake --build build --target intrinsic_tests
    ctest --test-dir build --output-on-failure

test-sanitize: configure-sanitize
    cmake --build build-sanitize --target intrinsic_tests
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 ctest --test-dir build-sanitize --output-on-failure

cppcheck: configure
    cppcheck \
    --project=build/compile_commands.json \
    --enable=all \
    --inconclusive \
    --check-level=exhaustive \
    --std=c++20 \
    --suppress=missingIncludeSystem \
    -q

run: build
    ./build/intrinsic

debug: build
    lldb ./build/intrinsic

release:
    cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build-release

run-release: release
    ./build-release/intrinsic

clean:
    rm -rf build build-release build-sanitize
