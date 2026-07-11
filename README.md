## Сборка и тесты

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```
Санитайзеры: `cmake -S . -B build -DDRWEB_SANITIZE=address,undefined`

### Windows

Visual Studio 2022 (17.8+) или 2026.
Задачи 1-2 собираются нативно.
задаче 3 нужен 32-битный тулчейн.

```bat
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release

cmake -S . -B build32 -A Win32
cmake --build build32 --config Release
ctest --test-dir build32 -C Release
```

## Контейнер

```sh
podman build -t drweb .
podman run --rm drweb
```

