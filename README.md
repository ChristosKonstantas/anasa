# ANASA - Asynchronous Neural Audio Scheduling Architecture

> Work in progress

A C++ experimental project for studying continuous, low-latency audio rendering and scheduling in a DAW-like timeline.

The current milestone focuses on the audio-pipeline foundation: a periodic audio-callback simulator, shared playback state, and bounded SPSC communication.

Planned components include:

- timeline-aware render scheduling;
- urgent, visible, and background work prioritization;
- asynchronous rendering with a fixed worker pool;
- buffering and underrun handling;
- seek and edit invalidation using stream generations and content versions;
- CPU DSP and neural-inference backends;
- later CPU/GPU scheduling experiments.

This is not an audio host or complete DAW. It is a controlled environment for developing and evaluating audio-rendering scheduling policies.