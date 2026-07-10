## Сборка и тесты

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```
Санитайзеры: `cmake -S . -B build -DDRWEB_SANITIZE=address,undefined`

## Контейнер

```sh
podman build -t drweb .
podman run --rm drweb
```
