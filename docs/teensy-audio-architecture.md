# Teensy Audio Library — How It Works

A high-level tour of the Teensy Audio Library's runtime model on Teensy 4.x.
This doc is intentionally light on details; ask for more depth where it matters.

## 1. The model in one sentence

You build a static graph of **audio objects** at startup; a periodic interrupt
pumps fixed-size **blocks of samples** through that graph, top to bottom,
once every ~2.9 ms.

## 2. The block

The fundamental unit of audio data is the **audio block**:

- **128 samples** per block (`AUDIO_BLOCK_SAMPLES`, configurable but rarely
  changed).
- **`int16_t`** mono samples, signed -32768..+32767.
- **44.1 kHz** sample rate (`AUDIO_SAMPLE_RATE_EXACT`).
- A block represents ~2.9 ms of audio (`128 / 44100`).

Stereo isn't a block property — it's two parallel blocks flowing through two
parallel signal paths. Every "stereo" object is actually a pair of mono
inputs/outputs.

## 3. Audio objects

Every node in the graph is an `AudioStream` subclass — `AudioSynthWaveform`,
`AudioMixer4`, `AudioFilterStateVariable`, `AudioOutputI2S`, etc. They each
implement one virtual method:

```cpp
virtual void update(void) = 0;
```

`update()` is where the DSP happens. It runs once per audio block. The
contract: read input blocks, do work, write output blocks. The library handles
*when* it gets called and *how* blocks arrive at its inputs.

Every object has a fixed number of input and output "ports" defined by its
constructor. Inputs are queues into which the library deposits one block per
period; outputs are slots from which the library hands a block to whatever's
connected.

## 4. The connection graph

`AudioConnection` objects wire outputs to inputs:

```cpp
AudioConnection patchCord(source, sourceOutputIndex, dest, destInputIndex);
```

The connection itself owns nothing audible — it's just a pointer-pair that
tells the library "when `source` writes its output[N], deliver it to
`dest`'s input[M]." All connections must be in scope for as long as the audio
chain is running, which is why you see `AudioConnection*` allocations with
explicit lifetime management in this project (`HybridSynthesizer.h`).

The graph is **static** in the library's model — every object and connection
that will ever participate exists from the start. There's no "add a voice on
the fly." You can fake dynamic behavior by routing a fixed pool of objects
through mixers and toggling mixer gains.

## 5. The update cycle

The Teensy core wires up a hardware timer at startup that fires a software
interrupt (`software_isr`) at exactly the block rate (44100 / 128 ≈ 344 Hz).
That ISR walks a global linked list of all `AudioStream` instances and calls
`update()` on each one whose `active` flag is set.

**Ordering**: objects are visited in the order they were constructed
(determined by static initialization order). For most graphs this is fine
because each `update()` reads its inputs and writes its outputs to internal
buffers — the library doesn't require updates to follow data-flow order. A
"downstream" object reading a block written this period will see *last
period's* output if its update runs before the source's. In practice the
constructors line up sensibly and people don't notice.

(More on this if you want to dig in — there's a subtle latency consequence
that depends on ordering.)

## 6. Block memory

Blocks aren't allocated on the fly during DSP — that would be too slow and
fragment-prone. Instead, at startup you call:

```cpp
AudioMemory(32);   // 32 audio blocks
```

That pre-allocates 32 fixed-size buffers from a pool. During `update()`, an
object that wants to produce output calls `allocate()` to grab a free block
from the pool, fills it, and the connection logic delivers it. Each block is
**reference-counted** — if a single output fans out to three inputs, three
input queues each hold a pointer to the same block, with a refcount of 3.
When each consumer's `update()` finishes, it calls `release()` and the refcount
drops; at zero, the block returns to the free pool.

**Practical consequence**: too few blocks and `allocate()` returns NULL
mid-graph, producing silence/glitches. Too many costs RAM (`128 samples ×
2 bytes ≈ 256 bytes per block`). `AudioMemoryUsageMax()` tells you the peak
simultaneous count, useful for sizing the pool. This project recently bumped
from 20 to 32 blocks because some chord-heavy moments were getting close.

## 7. CPU and latency

**CPU load** is measured by sampling the ARM cycle counter around each
`update()` call. `AudioProcessorUsage()` is "last block's wall-time
percentage"; `AudioProcessorUsageMax()` is the high-water mark since reset.
(Already covered in detail in our earlier conversation — see code in
`AudioStream.cpp` around `software_isr`.)

**Latency** has a floor of one block (~2.9 ms) on input and one on output —
something arriving at the I2S input shows up on the I2S output no faster
than two periods later, plus whatever the actual DAC and ADC pipelines
contribute. Update ordering within the graph can add more periods if a chain
of objects happens to be visited in reverse data-flow order. The library
doesn't reorder for you.

## 8. Where things live

In the Teensy framework:

- `cores/teensy4/AudioStream.h` / `.cpp` — base class, ISR, pool, scheduler.
- `libraries/Audio/` — all the concrete `AudioStream` subclasses (oscillators,
  filters, mixers, I/O). Each is typically one `.h` / `.cpp` pair named after
  the object.

In this project:

- `HybridSynthesizer.h` — the top-level audio graph: builds every voice
  engine, wires their outputs through mixer banks into `sumL/sumR`, taps
  peak monitors, then drops the sum into `AudioOutputI2S` (and optionally
  `AudioOutputUSB`).
- Per-engine graphs: `DroneSynthesizer`, `StringPadSynthesizer`,
  `SoundfontSynthesizer`, `SoundfontPadSynthesizer`, `StringsSynthesizer`.

---

Tell me where you'd like to go deeper. Common follow-up directions:

- **Block lifecycle / refcount** — what happens when an object forgets to
  `release()`, how to debug a pool leak.
- **Update ordering and latency** — why constructor order matters and how to
  spot a single-period delay in a chain.
- **Mixer math** — how `AudioMixer4` actually combines its inputs, what gain
  values larger than 1.0 mean, where clipping shows up.
- **DMA and I2S** — how `AudioOutputI2S` keeps the DAC fed asynchronously
  from the update ISR.
- **Memory cost per object** — sizing `AudioMemory()` from first principles
  rather than trial and error.
- **Reading vs. consuming inputs** — the `receiveReadOnly` / `receiveWritable`
  distinction and why some objects can corrupt blocks for fan-out siblings.
