# Game Server (C++20 / Boost.Asio + Beast / PostgreSQL)

HTTP‑сервер игрового бэкенда: раздаёт статические файлы и предоставляет JSON API для многопользовательской игры про «собак», которые бегают по карте, собирают предметы и набирают очки. Игровой мир описывается в конфигурации JSON. Сервер поддерживает пошаговый режим (ручные тики через API) и авто‑тик (таймер на сервере), а также опциональное сохранение/восстановление состояния.

## Возможности

- **HTTP API**: карты, подключение игрока, список игроков, состояние игры, управление движением, таблица рекордов
- **Статика**: раздача файлов из директории. Флаг `--www-root`
- **Тики**:
  - авто‑тик `--tick-period` (сервер сам вызывает `Tick` по таймеру)
  - ручной тик через `POST /api/v1/game/tick` (когда авто‑тик не включён)
- **Сохранение состояния**: `--state-file` + опционально `--save-state-period`
- **PostgreSQL**: хранение «рекордов» (retired players) в таблице `retired_players` (создаётся автоматически при старте)

## Зависимости

- C++20
- Boost: Asio, Beast, JSON, Program Options, Serialization
- PostgreSQL + libpqxx

## Сборка (пример через CMake)

Пример `CMakeLists.txt`, для сборки проекта. Пути/пакеты могут отличаться в зависимости от системы.

```cmake
cmake_minimum_required(VERSION 3.20)
project(game_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS program_options serialization json)
find_package(Threads REQUIRED)
find_package(PQXX REQUIRED)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(game_server ${SRC})
target_include_directories(game_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(game_server PRIVATE
    Threads::Threads
    Boost::program_options
    Boost::serialization
    PQXX::pqxx
)

```

## Запуск

Сервер слушает `0.0.0.0:8080`.

Обязательные вещи:
- переменная окружения `GAME_DB_URL` (строка подключения к PostgreSQL)
- `--config-file` (JSON с описанием карт/параметров игры)
- `--www-root` (директория со статикой)

Пример:

```bash
export GAME_DB_URL="postgresql://user:password@localhost:5432/game"

./game_server \
  --config-file ./data/config.json \
  --www-root ./www \
  --tick-period 50 \
  --randomize-spawn-points
```

Если `--tick-period` не задан, сервер работает в ручном режиме, и тики нужно отправлять через API.

### Параметры командной строки

- `-h, --help` — справка
- `-c, --config-file <file>` — путь к конфигурации игры
- `-w, --www-root <dir>` — директория со статическими файлами
- `-t, --tick-period <ms>` — период тика в миллисекундах (включает авто‑тик)
- `--randomize-spawn-points` — случайный спавн игроков
- `-f, --state-file <file>` — путь к файлу состояния (вкл. сохранение/восстановление)
- `-p, --save-state-period <ms>` — период автосохранения состояния (работает только вместе с `--state-file`)

## Конфигурация игры (JSON)

Файл, который передаётся через `--config-file`, парсится модулем `src/configuration/json_loader.*`.

Верхний уровень:
- `maps`: массив карт
- `defaultDogSpeed` (опционально)
- `defaultBagCapacity` (опционально)
- `dogRetirementTime` (опционально, секунды; если <= 0, используется 60)
- `lootGeneratorConfig`: объект `{ "period": <ms>, "probability": <double> }`

Карта (`maps[*]`):
- `id`: строка
- `name`: строка
- `roads`: массив объектов `{x0,y0,x1}` или `{x0,y0,y1}`
- `buildings`: массив `{x,y,w,h}`
- `offices`: массив `{id,x,y,offsetX,offsetY}`
- `lootTypes`: массив объектов (метаданные лута). Используются поля:
  - `name`: строка
  - `value`: int
  - остальные поля сохраняются как метаданные и отдаются через API карты
- `dogSpeed` (опционально, переопределяет `defaultDogSpeed`)
- `bagCapacity` (опционально, переопределяет `defaultBagCapacity`)

## API (v1)

Все API‑эндпоинты начинаются с `/api/v1`.

### Карты

- `GET /api/v1/maps` — список карт: `[{"id":"...","name":"..."}, ...]`
- `GET /api/v1/maps/<id>` — полное описание карты (включая `lootTypes` метаданные)

### Игровые эндпоинты

- `POST /api/v1/game/join` — подключиться к игре
  - body: `{ "userName": "<name>", "mapId": "map1" }`
  - response: `{ "authToken": "<32 hex>", "playerId": <int> }`

- `GET /api/v1/game/players` — игроки на карте
  - header: `Authorization: Bearer <token>`

- `GET /api/v1/game/state` — состояние игры (игроки + потерянные объекты)
  - header: `Authorization: Bearer <token>`

- `POST /api/v1/game/player/action` — управление движением
  - header: `Authorization: Bearer <token>`
  - body: `{ "move": "U"|"D"|"L"|"R"|"" }` (`""` — остановка)

- `POST /api/v1/game/tick` — ручной тик
  - доступен **только если** сервер запущен без `--tick-period`
  - body: `{ "timeDelta": <ms> }`

- `GET /api/v1/game/records?start=N&maxItems=M` — таблица рекордов (M <= 100)

### Примеры curl

```bash
# список карт
curl -s http://localhost:8080/api/v1/maps

# join
TOKEN=$(curl -s -X POST http://localhost:8080/api/v1/game/join \
  -H 'Content-Type: application/json' \
  -d '{"userName":"User","mapId":"map1"}' | python -c "import sys,json; print(json.load(sys.stdin)['authToken'])")

# state
curl -s http://localhost:8080/api/v1/game/state -H "Authorization: Bearer $TOKEN"

# move up
curl -s -X POST http://localhost:8080/api/v1/game/player/action \
  -H "Authorization: Bearer $TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"move":"U"}'

# manual tick (если авто-тик выключен)
curl -s -X POST http://localhost:8080/api/v1/game/tick \
  -H 'Content-Type: application/json' \
  -d '{"timeDelta":50}'
```

## Сохранение и восстановление состояния

Если задан `--state-file`, сервер:
- при старте попробует загрузить состояние из файла (если файл существует)
- будет сохранять состояние на сигнал `SIGINT/SIGTERM` (перед остановкой)
- может сохранять состояние периодически, если задан `--save-state-period`

Сериализация сделана через Boost.Serialization (text archive).

## Структура проекта

Код находится в `src/`:

- `server/` — HTTP сервер (Boost.Beast) и `Ticker`
- `request_processing/` — роутинг `/api` + обработка статики
- `configuration/` — загрузка конфигурации игры из JSON, конвертация карты в JSON
- `game_model/` — модель игры: карты, сессии, собаки, лут, коллизии
- `app/` — слой приложения: игроки, токены, тик, интеграция с БД, состояния
- `postgres/` — connection pool + unit of work + репозиторий рекордов
- `infrastructure/` — сохранение/восстановление состояния
- `detail/` — утилиты (логгер, random, tagged types и т.п.)

## Примечания

- Сервер ожидает `GAME_DB_URL` в окружении. Если переменная не задана — завершится с ошибкой.
- Таблица и индекс для рекордов создаются автоматически при старте (`CREATE TABLE IF NOT EXISTS ...`).
