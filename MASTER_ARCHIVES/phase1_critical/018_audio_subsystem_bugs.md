# Issue #018: Audio Subsystem — 6 security/correctness bugs

**Severity:** Critical (1 command injection), High (3 correctness), Medium (2)  
**Phase:** 1 (Critical)  
**Files:** `audio/voice_system.cpp`, `audio/audio_system.cpp`, `audio/wht_comm.cpp`, `audio/wht_scaled.cpp`

---

## Bug 1: Command injection in voice_speak (Critical)

**File:** `audio/voice_system.cpp:145-150`

```cpp
char cmdline[8192];
snprintf(cmdline, sizeof(cmdline), "piper --volume %s --model %s --text \"%s\"",
         vol_arg, model_path, text);
CreateProcessA("piper.exe", cmdline, ...);
```

`text` is user-provided speech content (e.g., transcribed by Whisper from an untrusted voice input). On Windows, double-quote injection via `text` gives arbitrary command execution:

**Example exploit input:** `"; cmd.exe /c "malicious command";`

The `is_safe_path()` check on line 136 only validates `model_path`, not `text`. The Unix path (line 159-163) uses `execlp` with separate arguments, which is safe.

**Fix:** Validate `text` contains only printable characters, no quotes/semicolons/pipes. Or use `_spawnv` / `CreateProcess` with separate argument array to avoid cmdline injection entirely. Mark the Windows CreateProcess path with a prominent security warning.

---

## Bug 2: ma_sound resource leak in audio_play_sfx (High)

**File:** `audio/audio_system.cpp:73-90`

```cpp
ma_sound sfx;
ma_sound_init_from_file(..., &sfx);   // allocates internal resources
ma_sound_start(&sfx);
return 0;                              // sfx never uninitialized — resource leak
```

Every call to `audio_play_sfx` leaks a `ma_sound` object (including its internal buffer, decoder, and node graph resources). The comment on lines 85-88 acknowledges this ("small leak") but does not track or clean up completed sounds.

**Fix:** Maintain a `std::vector<ma_sound>` of active SFX sounds. In `audio_update()`, iterate and `ma_sound_uninit()` any that have finished playing (`ma_sound_at_end()`), then erase them.

---

## Bug 3: WHT processing path mathematically wrong (High)

**File:** `audio/audio_system.cpp:241-281`

The WHT path (lines 260-281) purports to implement convolution but does not. Lines 253-256 contain a TODO acknowledging this:

```
// The current implementation does an extra ifwht + divide-by-N
// that transforms the result back to the Hadamard domain
// scaled by 1/(N√N) instead of properly implementing WHT-based convolution
```

The correct WHT convolution formula is:
```
y = ifwht( fwht(x) ⊙ fwht(h) ) / N
```

But this code does:
```
for each out: output_wht[out] += input_wht[i] * s_wht_weights[out][i];
ifwht(output_wht);
divide each by N;
```

This is an O(N²) matrix multiply in the Hadamard domain followed by an extra ifwht, producing incorrect results.

Additionally, the fallback path (lines 244-249) uses `s_wht_weights` even when `s_wht_initialized` is false — the static array is zero-initialized, producing all-zero output blocks.

---

## Bug 4: ifwht_fp uses integer division instead of fixed-point division (Medium)

**File:** `audio/wht_scaled.cpp:28`

```cpp
data[i] = vn::fp20_t::from_raw(data[i].raw() / n);
```

`data[i].raw() / n` performs plain integer division on the raw `int64_t` value. This is not the same as fixed-point division by `n`. While the numerical error is small (~1/2^20 per element), it breaks the self-inverse property:

```cpp
fwht_fp(data, log2n, false);  // butterfly only
// Now WHT(data) is in data
for (int i = 0; i < n; i++) {
    data[i] = vn::fp20_t::from_raw(data[i].raw() / n);  // wrong: integer truncation
}
```

**Fix:** Use the proper fixed-point division:  
```cpp
data[i] = data[i] / vn::fp20_t(static_cast<float>(n));
```

---

## Bug 5: wht_comm.cpp null pointer / buffer overflow risk (Medium)

**File:** `audio/wht_comm.cpp:5-10`

```cpp
void encode_message(const char* message, float* coeffs) {
    int len = strlen(message);  // null pointer: crash if message == nullptr
    for (int i = 0; i < len; i++) {
        coeffs[i] = (float)message[i];  // buffer overflow: writes beyond WHT_N elements
    }
```

No null check on `message`, no length bound. Messages longer than `WHT_N` (32) overflow the `coeffs` buffer.

**Fix:** Add null check and `len = min(len, WHT_N)`.

---

## Bug 6: audio_init_voice uses sizeof(struct) instead of sizeof(struct VoiceSystem) (Medium)

**File:** `audio/audio_system.cpp:138`

```cpp
VoiceSystemHandle ctx = (VoiceSystemHandle)malloc(sizeof(struct VoiceSystem));
```

`struct VoiceSystem` is defined as `{ int placeholder; }` (line 127-129), so allocating `sizeof(struct VoiceSystem)` = 4 bytes. But the function name and usage imply this should be a real voice system implementation. The previous (correct) pattern is in `voice_system.cpp:26` which allocates `sizeof(VoiceSystemImpl)` with real fields. This is likely a placeholder that will need to be replaced with the full impl size when integration is completed.
