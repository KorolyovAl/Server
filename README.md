# Game Server (C++20, Boost.Asio/Beast, PostgreSQL)

HTTP backend for a simple multiplayer game. The server serves static files from a configured document root and exposes a JSON API for gameplay: players join a map, move a character, collect loot, and accumulate score. The game world is defined in a JSON configuration.

The server supports two update modes:
- **automatic ticks** (server-driven timer)
- **manual ticks** via an API endpoint

Optional **state persistence** is available to save and restore the current server state.

## Features

- **HTTP API (JSON)**: maps, join/auth, players list, game state, movement, records
- **Static file serving** from `--www-root`
- **Ticking**:
  - automatic: `--tick-period` (server calls `Tick` on a timer)
  - manual: `POST /api/v1/game/tick` (when automatic ticking is disabled)
- **State save/restore**: `--state-file` with optional `--save-state-period`
- **PostgreSQL records storage**: retired players are stored in a `retired_players` table (created on startup)

## Requirements

- C++20 compiler
- Boost (Asio, Beast, JSON, Program Options, Serialization)
- PostgreSQL and **libpqxx**

## Build (example with CMake)

A `CMakeLists.txt` example for building a single executable is shown below. Package names and component lists may differ by platform/distribution.

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

## Run

Default listen address: `0.0.0.0:8080`.

Required inputs:
- `GAME_DB_URL` environment variable (PostgreSQL connection string)
- `--config-file` (JSON world configuration)
- `--www-root` (directory with static assets)

Example:

```bash
export GAME_DB_URL="postgresql://user:password@localhost:5432/game"

./game_server \
  --config-file ./data/config.json \
  --www-root ./www \
  --tick-period 50 \
  --randomize-spawn-points
```

If `--tick-period` is not specified, the server runs in manual mode and ticks must be driven via `POST /api/v1/game/tick`.

### Command-line options

- `-h, --help` — help
- `-c, --config-file <file>` — game configuration JSON
- `-w, --www-root <dir>` — static files root
- `-t, --tick-period <ms>` — tick period in milliseconds (enables automatic ticking)
- `--randomize-spawn-points` — randomize player spawn locations
- `-f, --state-file <file>` — state file path (enables save/restore)
- `-p, --save-state-period <ms>` — periodic autosave interval (requires `--state-file`)

## Game configuration (JSON)

The file provided via `--config-file` is parsed by `src/configuration/json_loader.*`.

Top-level fields:
- `maps`: array of map objects
- `defaultDogSpeed` (optional)
- `defaultBagCapacity` (optional)
- `dogRetirementTime` (optional, seconds; if `<= 0`, a default value of 60 is used)
- `lootGeneratorConfig`: object `{ "period": <ms>, "probability": <double> }`

Map object (`maps[*]`):
- `id`: string
- `name`: string
- `roads`: array of `{x0,y0,x1}` or `{x0,y0,y1}`
- `buildings`: array of `{x,y,w,h}`
- `offices`: array of `{id,x,y,offsetX,offsetY}`
- `lootTypes`: array of loot type metadata objects; the following fields are used:
  - `name`: string
  - `value`: int
  - any additional fields are preserved as metadata and returned by the map API
- `dogSpeed` (optional; overrides `defaultDogSpeed`)
- `bagCapacity` (optional; overrides `defaultBagCapacity`)

## API (v1)

All endpoints are prefixed with `/api/v1`.

### Maps

- `GET /api/v1/maps` — map list: `[{"id":"...","name":"..."}, ...]`
- `GET /api/v1/maps/<id>` — full map description (includes `lootTypes` metadata)

### Game endpoints

- `POST /api/v1/game/join` — join a game session
  - request body: `{ "userName": "<name>", "mapId": "map1" }`
  - response: `{ "authToken": "<32 hex>", "playerId": <int> }`

- `GET /api/v1/game/players` — players on the current map
  - header: `Authorization: Bearer <token>`

- `GET /api/v1/game/state` — current game state (players and dropped objects)
  - header: `Authorization: Bearer <token>`

- `POST /api/v1/game/player/action` — movement control
  - header: `Authorization: Bearer <token>`
  - request body: `{ "move": "U"|"D"|"L"|"R"|"" }` (`""` means stop)

- `POST /api/v1/game/tick` — manual tick
  - available **only when** the server is started without `--tick-period`
  - request body: `{ "timeDelta": <ms> }`

- `GET /api/v1/game/records?start=N&maxItems=M` — records table (`M <= 100`)

### curl examples

```bash
# list maps
curl -s http://localhost:8080/api/v1/maps

# join
TOKEN=$(curl -s -X POST http://localhost:8080/api/v1/game/join \
  -H 'Content-Type: application/json' \
  -d '{"userName":"User","mapId":"map1"}' \
  | python -c "import sys,json; print(json.load(sys.stdin)['authToken'])")

# state
curl -s http://localhost:8080/api/v1/game/state -H "Authorization: Bearer $TOKEN"

# move up
curl -s -X POST http://localhost:8080/api/v1/game/player/action \
  -H "Authorization: Bearer $TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"move":"U"}'

# manual tick (only when automatic ticking is disabled)
curl -s -X POST http://localhost:8080/api/v1/game/tick \
  -H 'Content-Type: application/json' \
  -d '{"timeDelta":50}'
```

## State persistence

When `--state-file` is provided, the server:
- attempts to load state from the file on startup (if the file exists)
- saves state on `SIGINT`/`SIGTERM` before shutdown
- can save periodically when `--save-state-period` is set

Serialization is implemented with Boost.Serialization (text archive).

## Project layout

Source code is located in `src/`:

- `server/` — HTTP server (Boost.Beast) and `Ticker`
- `request_processing/` — `/api` routing and static file handling
- `configuration/` — JSON config loading and map-to-JSON conversion
- `game_model/` — game model: maps, sessions, dogs, loot, collision detection
- `app/` — application layer: players, tokens, tick orchestration, DB integration, state
- `postgres/` — connection pool, unit of work, records repository
- `infrastructure/` — state save/restore components
- `detail/` — utilities (logger, random generator, tagged types, etc.)

## Notes

- `GAME_DB_URL` must be set; otherwise the server exits with an error.
- Records table and related indexes are created on startup using `CREATE TABLE IF NOT EXISTS ...`.
