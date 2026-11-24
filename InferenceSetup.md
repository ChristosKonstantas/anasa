## Neural inference: Windows x64 CPU setup

ANASA optionally runs TIGER v106 singing-voice inference through ONNX Runtime.
`OpenVpiRenderer` implements `IPhraseRenderer`. `PhraseTileRenderer` connects
whole-phrase inference to the existing scheduler and executor.

The current adapter targets TIGER v106, the `fresh` voice embedding, and its
bundled HiFi-GAN vocoder. It is not a general loader for arbitrary DiffSinger
voicebanks.

Output is 44,100 Hz audio with identical left and right channels. The model
produces mono audio, which ANASA duplicates into two channels.

### Prerequisites

- Windows x64.
- Visual Studio 2022 or Build Tools with "Desktop development with C++".
- CMake 3.22 or newer.
- Git, PowerShell, and `curl.exe`.
- ONNX Runtime 1.31.1 native CPU libraries.
- TIGER v106 voicebank.

The commands below run from the repository root. Python, OpenUTAU, model
training, and ONNX export are not required for this integration.

For a fresh checkout:

```powershell
git clone https://github.com/ChristosKonstantas/anasa.git
cd anasa
```

Prepare local dependency directories:

```powershell
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path ".\local-deps", ".\models" | Out-Null
```

### Install ONNX Runtime

Download Microsoft's pinned native package:

```powershell
curl.exe --fail --location "https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/1.31.1/microsoft.ml.onnxruntime.1.31.1.nupkg" --output ".\local-deps\onnxruntime-1.31.1.zip"
if ($LASTEXITCODE -ne 0) { throw "ONNX Runtime download failed" }
```

A NuGet package is a ZIP archive. Extract it:

```powershell
Expand-Archive -LiteralPath ".\local-deps\onnxruntime-1.31.1.zip" -DestinationPath ".\local-deps\onnxruntime-package-1.31.1" -Force
```

Arrange the native headers and Windows x64 libraries in the SDK layout
expected by ANASA's CMake configuration:

```powershell
$ortRoot = Join-Path (Get-Location).Path "local-deps/onnxruntime-win-x64-1.31.1"

New-Item -ItemType Directory -Force -Path "$ortRoot/include", "$ortRoot/lib" | Out-Null

Copy-Item -Path ".\local-deps\onnxruntime-package-1.31.1\build\native\include\*" -Destination "$ortRoot/include" -Recurse -Force

Copy-Item -Path ".\local-deps\onnxruntime-package-1.31.1\runtimes\win-x64\native\*" -Destination "$ortRoot/lib" -Recurse -Force

Copy-Item -LiteralPath ".\local-deps\onnxruntime-package-1.31.1\LICENSE" -Destination "$ortRoot/LICENSE" -Force
```

This uses the package's native C/C++ components; it does not require a
.NET project or the .NET SDK.

Source: [Microsoft.ML.OnnxRuntime 1.31.1](https://www.nuget.org/packages/Microsoft.ML.OnnxRuntime/1.31.1).

### Download TIGER v106

Read the [upstream voicebank information and terms](https://github.com/spicytigermeat/tiger_diffsinger)
before using the model. Preserve the license included in the downloaded
voicebank. Model licensing is separate from ANASA's source-code license.

Download the pinned voicebank pack:

```powershell
curl.exe --fail --location "https://github.com/spicytigermeat/tiger_diffsinger/releases/download/v106/TIGER_DS_v106_PACK.zip" --output ".\local-deps\TIGER_DS_v106_PACK.zip"
if ($LASTEXITCODE -ne 0) { throw "TIGER download failed" }
```

The pack contains another ZIP holding the voicebank. Extract both layers:

```powershell
Expand-Archive -LiteralPath ".\local-deps\TIGER_DS_v106_PACK.zip" -DestinationPath ".\local-deps\tiger-pack" -Force

Expand-Archive -LiteralPath ".\local-deps\tiger-pack\TIGER_DS_v106_PACK\Voice Library\TIGER_DS_v106.zip" -DestinationPath ".\models\tiger-v106" -Force
```

The renderer loads these files relative to the voicebank directory:

| Required file | Purpose |
| --- | --- |
| `dsacoustic/acoustic.onnx` | Acoustic model |
| `dsacoustic/phonemes.txt` | Phoneme vocabulary |
| `dsacoustic/tiger_fresh.emb` | Selected voice embedding |
| `dsvocoder/tgm_hifigan.onnx` | Waveform vocoder |

Check that extraction produced the expected paths:

```powershell
$modelFiles = @(
    "dsacoustic/acoustic.onnx",
    "dsacoustic/phonemes.txt",
    "dsacoustic/tiger_fresh.emb",
    "dsvocoder/tgm_hifigan.onnx"
)

foreach ($relativePath in $modelFiles) {
    $modelPath = Join-Path ".\models\tiger-v106" $relativePath
    if (!(Test-Path -LiteralPath $modelPath -PathType Leaf)) {
        throw "Missing model file: $modelPath"
    }
}
```

The full voicebank includes additional models and configuration files.
The current ANASA adapter loads only the four files listed above and
supplies phoneme durations and pitch explicitly.

`local-deps/`, `models/`, and `build/` are excluded by `.gitignore`.

### Configure and build

Configure a dedicated Visual Studio x64 build:

```powershell
cmake -S . -B build/neural-runtime -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DOPENVPI=ON -DNN_BENCHMARK=ON "-DONNXRUNTIME_ROOT=$ortRoot"
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }
```

Build Release:

```powershell
cmake --build build/neural-runtime --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed" }
```

The build copies the ONNX Runtime DLLs beside the executables.

`OPENVPI` enables the neural backend.
`NN_BENCHMARK` additionally builds the standalone inference benchmark.
Neither requires the synthetic benchmark option `BENCHMARKS`.

### First check: render and listen to the complete timeline

Run one trial per scheduling policy:

```powershell
.\build\neural-runtime\runtime-demo\Release\main.exe listen ".\models\tiger-v106" 6 10 1 ".\build\neural-runtime\listen-t6-s10"
if ($LASTEXITCODE -ne 0) { throw "Neural playback demo failed" }
```

The arguments after the voicebank are:

| Argument | Example | Meaning |
| --- | ---: | --- |
| Threads | `6` | ONNX Runtime intra-op threads |
| Steps | `10` | Model inference step count |
| Trials | `1` | Trials per scheduling policy |
| Output directory | Path | Recording destination |

Six threads is an example configuration, not a universally optimal setting.
Lower step counts trade synthesis quality against rendering cost.

The neural executor uses one worker. ONNX threads cooperate on each
whole-phrase inference request.

`listen` runs FIFO and Priority separately over six prepared phrases,
approximately 11.98 seconds of timeline audio. It saves the simulated
callback output, including startup preparation silence.

Open the Priority recording:

```powershell
Start-Process ".\build\neural-runtime\listen-t6-s10\neural\playback-Priority-1.wav"
```

The demo writes WAV files; it does not stream directly to an audio device.

### Measure standalone inference

Run five measured phrase renders:

```powershell
.\build\neural-runtime\apps\Release\anasa-inference-benchmark.exe ".\models\tiger-v106" 6 10 5 ".\build\neural-runtime\benchmark-t6-s10.wav"
if ($LASTEXITCODE -ne 0) { throw "Inference benchmark failed" }
```

The benchmark reports model setup, the first render, acoustic-model time,
vocoder time, total phrase time, and rendering real-time factor.

The first render and an additional warm-up render precede the measured
trials. The WAV contains the last measured phrase.

This benchmark measures serial inference. Its PCM tile-delivery measurement
does not include neural inference.

### Run scheduling scenarios

Compare seek, viewport preparation, and edit recovery using neural inference:

```powershell
.\build\neural-runtime\runtime-demo\Release\main.exe neural ".\models\tiger-v106" 6 10 5 ".\build\neural-runtime\neural-t6-s10"
```

Run the synthetic scenarios:

```powershell
.\build\neural-runtime\runtime-demo\Release\main.exe synthetic 5 1000 ".\build\neural-runtime\synthetic-w1000"
```

Run both suites sequentially:

```powershell
.\build\neural-runtime\runtime-demo\Release\main.exe both ".\models\tiger-v106" 6 10 5 ".\build\neural-runtime\both-t6-s10" 1000
```

Use separate output directories for different configurations to preserve
their recordings. Running the same configuration into the same directory
replaces matching WAV files.

### Current input and metric limitations

- The examples prepare "la la la" explicitly. There is no arbitrary-text
  input, MIDI-file importer, or automatic lyric-to-note alignment yet.
- `SingingRequest` contains phonemes, durations, and a frame-level pitch
  curve. The current prepared timeline uses equal-length phrases.
- The edit scenario selects a prepared revision transposed by three
  semitones.
- Recovery measures observed playback readiness, not the first audible
  sample.
- Underruns count active-playback callbacks lacking the required valid
  audio block. Preparation pauses are excluded.
- Maximum callback duration measures callback execution, not wake-up
  lateness or hardware-device deadline misses.
- Expected cancellation can produce ONNX termination messages, including
  when remaining work is cancelled during shutdown. Inspect cancellation
  and failure counters separately.