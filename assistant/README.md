# ModularS2S — Jetson Orin Nano Setup

Headless C++ Speech2Speech assistant. This guide takes a bare Jetson Orin
Nano to a running voice/typed assistant. See `../OVERVIEW.md` for the
project vision and design.

## Contents

1. [Architecture](#1-architecture)
2. [Prerequisites](#2-prerequisites)
3. [Set up jetson-containers](#3-set-up-jetson-containers)
4. [Bring up Ollama (LLM)](#4-bring-up-ollama-llm)
5. [Bring up wyoming-whisper (STT) and wyoming-piper (TTS)](#5-bring-up-wyoming-whisper-stt-and-wyoming-piper-tts)
6. [Install native build dependencies](#6-install-native-build-dependencies)
7. [Build the assistant](#7-build-the-assistant)
8. [Configure](#8-configure)
9. [Run](#9-run)
10. [Troubleshooting](#10-troubleshooting)
11. [Updating](#11-updating)

---

## 1. Architecture

Three GPU-accelerated services run as Docker containers (built/pulled via
`jetson-containers`); the assistant itself runs **natively on the Jetson
host**, not in a container, because it needs direct ALSA/PortAudio access
to the mic and speaker.

```
                    +-------------------+
   mic/keyboard --> |  assistant (C++)  | --> speaker
                    |  native on host   |
                    +---------+---------+
                              |
        +---------------------+---------------------+
        |                     |                      |
        v                     v                      v
  http://localhost:11434  127.0.0.1:10300       127.0.0.1:10200
  +---------------+       +----------------+    +---------------+
  |    ollama     |       | wyoming-whisper|    |  wyoming-piper|
  |   (LLM, GPU)  |       |  (STT, GPU)    |    |  (TTS, GPU)   |
  +---------------+       +----------------+    +---------------+
```

None of the three containers need `/dev/snd` — they only ever exchange raw
PCM bytes over localhost TCP with the assistant. The assistant never touches
CUDA/GGML/onnxruntime directly.

| Service | Role | Endpoint (default) | Image |
|---|---|---|---|
| `ollama` | LLM | `http://localhost:11434` | `dustynv/ollama:r36.4.0` |
| `wyoming-whisper` | STT | `127.0.0.1:10300` (Wyoming protocol) | `dustynv/wyoming-whisper` |
| `wyoming-piper` | TTS | `127.0.0.1:10200` (Wyoming protocol) | `dustynv/wyoming-piper` |

---

## 2. Prerequisites

- **Hardware**: NVIDIA Jetson Orin Nano.
- **OS**: JetPack 6.x (L4T r36.x). This project targets `r36.4.0` per the
  currently-running `dustynv/ollama:r36.4.0` container; other r36.x hosts
  are cross-compatible with the r36.2.0-tagged sidecar images (see
  [Troubleshooting](#10-troubleshooting) if a tag doesn't run).
- **Storage**: allocate at least ~30GB free for container images + models.
  If you're tight on space, mount swap — see jetson-containers'
  [`docs/setup.md`](../jetson-containers/docs/setup.md#mounting-swap).
- **Docker**: installed with the NVIDIA container runtime, and your user in
  the `docker` group (see step 3).
- **Audio**: a USB/onboard mic and speaker attached to the Jetson if you
  plan to use `--mode voice`. Not required for `--mode typed`.

---

## 3. Set up jetson-containers

`jetson-containers` is already vendored in this repo at `../jetson-containers`.

```bash
cd ../jetson-containers
bash install.sh   # prompts for sudo; adds `autotag`/`jetson-containers` to $PATH
```

Add your user to the `docker` group so you don't need `sudo` for every
container command (log out/in, or start a new shell, afterward):

```bash
sudo usermod -aG docker $USER
```

Confirm the NVIDIA runtime is the default (needed so containers can see the
GPU):

```bash
sudo docker info | grep 'Default Runtime'   # should print: Default Runtime: nvidia
```

If it doesn't, follow jetson-containers'
[Docker Default Runtime](../jetson-containers/docs/setup.md#docker-default-runtime)
steps.

---

## 4. Bring up Ollama (LLM)

Start the Ollama server as a background container (models are cached under
`jetson-containers/data/models/ollama` automatically):

```bash
jetson-containers run --name ollama -d $(autotag ollama)
```

Pull a model that fits the Orin Nano's shared 8GB memory budget (see the
[model-size note](#8-configure) below):

```bash
docker exec -it ollama /bin/ollama pull qwen3:4b
```

Verify it's serving:

```bash
curl http://localhost:11434/api/tags
```

You can also drop into an interactive `ollama run qwen3:4b` shell inside the
container at any time for manual testing — this is exactly the workflow
described in the project overview (`./run.sh dustynv/ollama:r36.4.0`, then
`ollama run <model>` inside).

---

## 5. Bring up wyoming-whisper (STT) and wyoming-piper (TTS)

**Option A — `docker-compose` (recommended, brings up all three services):**

```bash
cd docker
docker compose -f docker-compose.jetson.yaml up -d
```

This starts `ollama`, `wyoming-whisper`, and `wyoming-piper` together with
sensible defaults (see the compose file for the exact env vars — model
size, voice, ports). If you already started `ollama` manually in step 4,
either stop that container first or comment out the `ollama` service in the
compose file.

**Option B — `jetson-containers run`, one at a time:**

```bash
jetson-containers run -d --name wyoming-whisper \
  -e WHISPER_MODEL=base-int8 -e WHISPER_LANGUAGE=en \
  $(autotag wyoming-whisper)

jetson-containers run -d --name wyoming-piper \
  -e PIPER_VOICE=en_US-lessac-medium -e PIPER_USE_CUDA=true \
  $(autotag wyoming-piper)
```

**Verify both are listening:**

```bash
nc -zv 127.0.0.1 10300 && echo "wyoming-whisper: OK"
nc -zv 127.0.0.1 10200 && echo "wyoming-piper: OK"
```

If `autotag` can't find a compatible image for your exact L4T version, it
will offer to build one locally: `jetson-containers build wyoming-whisper
wyoming-piper` (this takes a while the first time).

---

## 6. Install native build dependencies

The assistant binary builds and runs directly on the Jetson host (Ubuntu
22.04, aarch64) — not inside a container:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake portaudio19-dev
```

Make sure your user is in the `audio` group (needed for `--mode voice` to
open the mic/speaker):

```bash
groups $USER | grep -qw audio || sudo usermod -aG audio $USER
```

(Log out/in for group changes to take effect.)

---

## 7. Build the assistant

```bash
cd ..   # back to assistant/
cmake -B build -S .
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

CMake fetches `cpp-httplib`, `nlohmann/json`, `yaml-cpp`, and `spdlog`
automatically via `FetchContent` on first configure (needs network access).
PortAudio is the one system dependency, located via `pkg-config`.

---

## 8. Configure

Edit `config/default.yaml` (or copy it and pass `--config <path>`):

```yaml
llm:
  endpoint: http://localhost:11434
  model: qwen3:4b

stt:
  endpoint: 127.0.0.1:10300

tts:
  endpoint: 127.0.0.1:10200
  voice: en_US-lessac-medium

audio:
  sample_rate: 16000
  vad_rms_threshold: 0.02        # raise if the mic picks up background noise as speech
  vad_trailing_silence_ms: 700   # how long a pause ends an utterance
  allow_barge_in: false          # see note below

input:
  mode: typed   # or: voice
```

**Model-size budget**: an 8GB Orin Nano runs the LLM, whisper, and piper
models on the GPU concurrently. Start small — `qwen3:4b` or `llama3.2:3b`,
Whisper `base-int8` (not `medium`/`large`), a `medium`-quality (not `high`)
Piper voice — before scaling up.

**Barge-in**: interrupting the assistant mid-reply by talking over it is
**off** by default in voice mode, because there's no acoustic echo
cancellation — the mic is muted while the assistant is speaking so it
doesn't hear itself. Set `allow_barge_in: true` only if your mic/speaker
setup won't feed back (e.g. a headset, or a speaker well away from the mic).

---

## 9. Run

```bash
# hands-needed: type each turn, mic never opened
./build/assistant --mode typed --config config/default.yaml

# hands-free: mic + VAD + speech recognition, no typing
./build/assistant --mode voice --config config/default.yaml
```

`Ctrl-C` (or `/quit` in typed mode) ends the session.

Start with `--mode typed` even if voice is your end goal — it exercises the
whole Ollama → sentence-buffer → Piper → speaker pipeline without the mic
in the loop, so audio/model problems are easier to isolate.

---

## 10. Troubleshooting

**`error 801: operation not supported` / `cudaGetDeviceCount` fails inside a container**
The GPU device nodes are group-owned on Jetson. Use `jetson-containers run`
(not a bare `docker run`) — it auto-detects and adds the right group IDs.
See [run.md](../jetson-containers/docs/run.md#gpu-access-as-non-root-cuda-error-801)
for the manual `--group-add` steps if you're not using the launcher.

**`wyoming: failed to connect to 127.0.0.1:10300` (or `:10200`)**
The sidecar container isn't up yet or crashed. Check `docker ps` and
`docker logs wyoming-whisper` / `docker logs wyoming-piper`. First boot can
take a minute while it downloads the model/voice.

**No sound out of the speaker, or the assistant reads from the wrong mic**
List PortAudio's device indices (e.g. `python3 -c "import sounddevice;
print(sounddevice.query_devices())"` if you have it, or any PortAudio device
lister) and set `audio.input_device` / `audio.output_device` in the config
to the correct index instead of the default (`-1`).

**Assistant runs out of memory / Ollama gets killed under load**
You're likely running models too large for the concurrent LLM + STT + TTS
GPU footprint on an 8GB board — drop to smaller models (see
[Configure](#8-configure)) or mount swap per jetson-containers'
[setup docs](../jetson-containers/docs/setup.md#mounting-swap).

**Voice mode keeps triggering on background noise, or cuts off mid-sentence**
Tune `audio.vad_rms_threshold` (higher = less sensitive) and
`audio.vad_trailing_silence_ms` (higher = waits longer before deciding
you're done talking) in the config.

---

## 11. Updating

Pull newer sidecar images and restart:

```bash
cd docker
docker compose -f docker-compose.jetson.yaml pull
docker compose -f docker-compose.jetson.yaml up -d
```

Rebuild the assistant after pulling code changes:

```bash
cmake --build build -j$(nproc)
```
