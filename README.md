# Redis clone

A Redis-like in-memory key-value server written in [C++].

> **Status:** Learning project. Not production-ready and not compatible with the real Redis protocol (RESP).

## Features

- Single-threaded TCP server with a non-blocking event loop (`poll`)
- Custom length-prefixed binary protocol
- Tagged type-length-value (TLV) serialization for responses (nil, error, string, int, double, array)
- Hashtable with progressive rehashing (no latency spikes when resizing)
- Sorted sets backed by an AVL tree, with rank and range queries
- Idle connection timeouts using timers (intrusive doubly linked list)
- Key expiration (TTL) using a min-heap
- Thread pool for freeing large objects without blocking the event loop

## Requirements

- Linux or macOS ([Windows works via WSL])
- `make`

## Build

```bash
git clone git@github.com:DancingDog73/redis_in_c.git
cmake -B build 
cmake --build build 
```

## Usage

Start the server (default port [1234]):

```bash
./build/server
```

In another terminal, start the interactive client:

```bash
./build/client
```

```
> set foo bar
(nil)
> get foo
(str) bar
> zadd scores 1.5 alice
(int) 1
> zadd scores 2.5 bob
(int) 1
> zscore scores alice
(dbl) 1.5
> zquery scores 0 a 0 10
(arr) len=4
(str) alice
(dbl) 1.5
(str) bob
(dbl) 2.5
(arr) end
> pexpire foo 5000
(int) 1
> get nokey
(nil)
> quit
```

Type `quit`, `exit`, or press Ctrl-D to leave.

## Commands

| Command | Description |
|---|---|
| `get key` | Get the value of a key |
| `set key value` | Set a key |
| `del key` | Delete a key |
| `keys` | List all keys |
| `zadd key score name` | Add or update a sorted-set member |
| `zrem key name` | Remove a member |
| `zscore key name` | Get a member's score |
| `zquery key score name offset limit` | Range query ordered by (score, name) |
| `pexpire key ms` | Set a TTL in milliseconds |
| `pttl key` | Get remaining TTL (-1 if none, -2 if missing) |

## Architecture

- **Event loop:** one thread multiplexes all client sockets with `poll`, using non-blocking reads and writes with per-connection buffers.
- **Protocol:** each message is a 4-byte length followed by the payload. Responses use TLV-tagged values so the client can parse nested data.
- **Hashtable:** chained hashtable with intrusive nodes. Two tables are kept during a resize and entries migrate a few at a time.
- **Sorted set:** an AVL tree ordered by (score, name) plus a hashtable for name lookups. Nodes track subtree size to support rank-based offsets.
- **Timers:** idle connections are tracked in a linked list ordered by activity. The poll timeout is computed from the nearest deadline.
- **TTL:** a min-heap keyed by expiration time. Expired keys are removed in the event loop.
- **Thread pool:** large sorted sets are deleted on worker threads so the main loop never stalls.


## Testing

```bash
python3 src/11/test_cmds.py
```

## What I learned

- How a non-blocking event loop serves many clients on one thread
- Designing a wire protocol and serializing nested data
- Why progressive rehashing matters for tail latency
- Intrusive data structures and self-balancing trees
- Timers, TTL expiry, and offloading slow work to a thread pool

