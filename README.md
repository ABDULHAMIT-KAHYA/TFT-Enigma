
# TFT Engine

A high-performance, data-driven Teamfight Tactics simulation engine built in modern C++.

TFT Engine provides deterministic combat simulation, macro-gameplay modeling, AI self-play, scenario testing, content importing, and validation tooling designed for large-scale experimentation, balancing, and AI research.

---

## Overview

TFT Engine is a custom implementation of Teamfight Tactics game systems focused on simulation accuracy, reproducibility, and extensibility.

The project is designed to support:

* Combat simulation
* Macro economy simulation
* AI agents
* Monte Carlo decision making
* Self-play training
* Scenario-based testing
* Automated validation
* Live TFT data importing

Unlike traditional game projects, TFT Engine is built primarily as a simulation platform that can be used for research, balancing, strategy analysis, and autonomous agent development.

---

## Core Features

### Combat Engine

* Deterministic combat simulation
* Unit movement and targeting
* Ability casting system
* Damage calculation pipeline
* Item effects
* Trait synergies
* Board-based positioning
* Reproducible seeded execution

### Macro Layer Simulation

Simulation of TFT gameplay outside combat:

* Economy management
* Shop rolling
* Leveling decisions
* Unit purchasing
* Board progression
* Round management
* Strategic decision making

### AI Framework

Built-in AI systems for automated gameplay.

Features include:

* Rule-based decision agents
* Self-play execution
* Large-scale simulation
* Monte Carlo evaluation
* Strategy testing

### Validation System

Automated verification tools ensure engine correctness.

Includes:

* Combat validation suites
* Scenario testing
* Detailed logging
* Regression testing support

### Content Pipeline

Data-driven architecture with runtime loading.

Supported content:

* Champions
* Traits
* Items
* Abilities

Content can be imported from TFT data sources and converted into engine-ready formats.

---

## Architecture

```text
TFT Engine
│
├── Combat Engine
│   ├── Units
│   ├── Abilities
│   ├── Damage System
│   ├── Targeting
│   └── Validation
│
├── Macro Simulation
│   ├── Economy
│   ├── Shop System
│   ├── Round System
│   └── Player State
│
├── AI
│   ├── Self Play
│   ├── Monte Carlo Search
│   └── Decision Systems
│
├── Content
│   ├── Champions
│   ├── Traits
│   ├── Items
│   └── Importers
│
└── Tooling
    ├── Scenario Runner
    ├── Logging
    └── Validation
```

---

## Data Driven Design

All gameplay content is loaded from external data files.

The engine validates and loads:

* Champion definitions
* Ability definitions
* Trait definitions
* Item definitions

This allows rapid iteration without recompiling gameplay logic.

---

## Deterministic Simulation

TFT Engine uses explicit seed management to guarantee reproducible results.

This is critical for:

* AI training
* Automated testing
* Balance analysis
* Debugging
* Tournament simulation

Every simulation can be reproduced exactly from its seed.

---

## Command Line Usage

### Run Macro Simulation

```bash
TFTEngine
```

Runs a full AI-controlled TFT simulation.

---

### Combat Validation

```bash
TFTEngine --validate
```

Executes all combat validation tests.

---

### Self Play

```bash
TFTEngine --selfplay 1000
```

Runs 1000 self-play games.

---

### Monte Carlo Simulation

```bash
TFTEngine --mc
```

Enables Monte Carlo decision evaluation.

---

### Monte Carlo Debug Mode

```bash
TFTEngine --mc-debug
```

Runs Monte Carlo simulations with detailed diagnostics.

---

### Scenario Testing

```bash
TFTEngine --scenario scenarios/example.json
```

Loads and executes a predefined combat scenario.

---

### Import Live TFT Data

```bash
TFTEngine --import-live-tft
```

Imports champion, item, and trait information from supported TFT sources.

---

### Import Cached Traits

```bash
TFTEngine --import-cached-traits
```

Updates trait definitions from cached data.

---

### Import Cached Items

```bash
TFTEngine --import-cached-items
```

Updates item definitions from cached data.

---

## Research Applications

TFT Engine was designed with experimentation in mind.

Potential applications include:

* Reinforcement learning
* Self-play training
* Balance analysis
* Meta simulation
* Strategy evaluation
* Monte Carlo search research
* Tournament-scale simulation

---

## Technical Goals

* Modern C++ architecture
* Deterministic execution
* High simulation throughput
* Data-driven content
* AI-first design
* Extensive validation tooling
* Scalable experimentation

---

## Project Status

TFT Engine is under active development.

Current focus areas:

* Combat accuracy
* Expanded item support
* Trait system improvements
* AI decision quality
* Simulation performance
* Advanced self-play infrastructure

---

## License

This project is intended for educational, research, and simulation purposes.

Teamfight Tactics and all related intellectual property belong to [Riot Games](https://www.riotgames.com?utm_source=chatgpt.com).

TFT Engine is an independent simulation project and is not affiliated with or endorsed by Riot Games.
