# Jetson Voice Assistant (Headless)

## Vision

Build a **fully local**, **CLI-first**, **modular** voice assistant that behaves like an offline Alexa.

The project is **not** an LLM, STT or TTS implementation.

Instead, it orchestrates existing best-in-class components into one seamless pipeline.

Primary goals:

* Fully offline
* Headless (no GUI)
* Docker-friendly
* ARM64 / Jetson first
* Low latency
* Streaming everywhere
* Interruptible speech
* Modular providers
* Minimal dependencies

---

# Current Environment

Hardware

* NVIDIA Jetson Orin Nano

Current LLM runtime

* Ollama
* Running inside:

  * `dustynv/ollama:r36.4.0`
* Deployed using:

  * `jetson-containers`

Current API

```text
http://localhost:11434
```

Streaming API:

```http
POST /api/generate
```

---

# Overall Pipeline

```text
                    User

                     │
                     ▼

            ┌──────────────────┐
            │  Microphone       │
            └──────────────────┘

                     │

                     ▼

            Voice Activity Detection
           (or Whisper end-of-speech)

                     │

                     ▼

              Speech-to-Text
               whisper.cpp

                     │

               transcript

                     │

                     ▼

          Conversation Manager

                     │

                     ▼

                Ollama API

                     │

          streamed JSON tokens

                     │

                     ▼

           Sentence Buffer

                     │

          complete sentence

                     │

                     ▼

              Text-to-Speech
              Piper / Kokoro

                     │

                     ▼

             Audio Playback

                     │

                     ▼

                  Speaker
```

---

# Philosophy

This project owns only:

* orchestration
* streaming
* cancellation
* conversation
* configuration

It should NEVER implement

* speech recognition
* speech synthesis
* language models

Those are delegated to external tools.

---

# External Components

## 1. LLM

Provider:

Ollama

Current model examples:

* llama3.2:3b
* qwen3:4b

Communication:

HTTP Streaming API

Responsibilities:

* prompt completion
* tool calling
* structured output

---

## 2. Speech Recognition

Preferred provider

whisper.cpp

Why

* native
* GPU capable
* ARM support
* offline
* proven

Interface

Input

PCM audio

Output

UTF-8 transcript

Future providers

* faster-whisper
* Whisper server

---

## 3. Text-to-Speech

Phase 1

Piper

Reasons

* lightweight
* offline
* native

Future

Kokoro

Optional

XTTS

---

## 4. Wake Word

Not required initially.

Future options

* OpenWakeWord
* Porcupine

Pipeline

Mic

↓

Wake word

↓

Whisper

---

# Responsibilities

## Assistant

Main coordinator.

Owns:

* microphone
* STT
* LLM
* TTS
* playback
* memory

Runs event loop.

---

## Audio Input

Responsibilities

* microphone capture
* sample conversion
* buffering

Output

PCM frames

---

## Speech Recognizer

Responsibilities

* consume PCM
* detect end of speech
* transcribe

Output

string

---

## Conversation Manager

Maintains

System prompt

Conversation history

Tool results

Context trimming

Prompt generation

---

## Ollama Client

Responsibilities

HTTP client

Streaming parser

Cancellation

Timeout handling

Model selection

Receives

Prompt

Produces

Streaming text

---

## Sentence Buffer

Input

streaming text

Example

Hel

lo

.

How

are

you

?

Output

Hello.

How are you?

Responsibilities

Detect sentence boundaries.

Send completed sentences immediately.

Never wait for entire response.

---

## Speech Synthesizer

Responsibilities

Receive sentence

Generate speech

Queue playback

Supports interruption.

---

## Audio Output

Responsibilities

Play audio queue

Stop immediately

Flush queue

---

# Streaming Strategy

Never wait for full LLM completion.

Desired behaviour

LLM

"The"

↓

"The weather"

↓

"The weather today"

↓

"The weather today is"

↓

"The weather today is sunny."

Immediately send

"The weather today is sunny."

to TTS.

Continue receiving

"It"

↓

"It will"

↓

"It will remain warm."

Immediately synthesize second sentence.

Goal

Speech begins while LLM continues generating.

---

# Cancellation

Every component must support cancellation.

Scenario

User

Tell me about Mars.

AI

Mars is...

User interrupts

Actually Jupiter.

Expected

Stop playback.

Cancel Ollama request.

Clear sentence buffer.

Discard pending synthesis.

Restart recognition.

---

# Thread Model

Suggested architecture

Thread

Microphone

Thread

Speech recognition

Thread

LLM streaming

Thread

TTS generation

Thread

Playback

Communication

Lock-free queues or thread-safe queues.

---

# Conversation Memory

Maintain

System prompt

User messages

Assistant messages

Future

Summaries

Long-term memory

RAG

---

# Configuration

Example

```yaml
llm:
  endpoint: http://localhost:11434
  model: qwen3:4b

stt:
  provider: whisper.cpp
  model: base.en

tts:
  provider: piper
  voice: en_US-lessac-medium

audio:
  sample_rate: 16000
```

---

# Future Tool Calling

The assistant should expose tools independently of the LLM.

Example

User

Turn off the kitchen lights.

↓

LLM emits tool call

↓

MQTT

↓

Home Assistant

↓

Result

↓

LLM explains success.

Future tools

* Home Assistant
* MQTT
* Spotify
* Calendar
* Weather
* Filesystem
* Camera
* Timers

---

# Repository Structure

```text
assistant/

    CMakeLists.txt

    src/

        main.cpp

        assistant/

        audio/

        conversation/

        llm/

        stt/

        tts/

        tools/

        config/

        util/

    include/

    tests/

    docs/
```

---

# Dependencies

Core

* C++20
* CMake

Networking

* cpp-httplib OR Boost.Beast

JSON

* nlohmann/json

Logging

* spdlog

Formatting

* fmt

Audio

* PortAudio OR ALSA OR PipeWire

Concurrency

* std::thread
* std::jthread
* std::stop_token
* std::condition_variable

---

# Development Roadmap

## Phase 1

Typed input

↓

Ollama

↓

Streaming

↓

Piper

↓

Speaker

No microphone.

Validate streaming speech.

---

## Phase 2

Add whisper.cpp.

Replace keyboard with microphone.

End-to-end voice conversation.

---

## Phase 3

Interruptions

Conversation memory

Prompt management

Cancellation

---

## Phase 4

Wake word

OpenWakeWord

Hands-free operation.

---

## Phase 5

Tool calling

MQTT

Home Assistant

Timers

Music

Calendar

---

## Phase 6

Vision

Camera

Image understanding

---

# Non-Goals

This project will NOT:

* Train models
* Implement an LLM
* Implement speech recognition
* Implement TTS
* Build a web interface
* Require cloud services
* Depend on Home Assistant
* Depend on Python at runtime unless unavoidable

---

# Long-Term Goal

Produce a single headless executable that can be launched as:

```bash
assistant \
  --ollama http://localhost:11434 \
  --model qwen3:4b \
  --stt whisper.cpp \
  --tts piper
```

and immediately provide a private, streaming, interruptible voice assistant with minimal configuration.

