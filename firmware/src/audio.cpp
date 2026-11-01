#include "audio.h"

#include <Arduino.h>
#include <driver/i2s.h>
#include <atomic>

#include "pins.h"

namespace {
constexpr i2s_port_t kPort = I2S_NUM_0;
constexpr int kFrames = 128;  // 2.7 ms per block, 4 blocks queued in DMA

portMUX_TYPE targetLock = portMUX_INITIALIZER_UNLOCKED;
SynthTarget sharedTarget;  // muted until the control side says otherwise
std::atomic<float> lastPeak{0}, loadPct{0};
std::atomic<uint32_t> stalls{0};

void audioTask(void*) {
  // Built here, not as a global: on ESP-IDF 4.4 the FPU may only be used once
  // a task runs, and a global constructor using floats panics at boot.
  static Synth synth(kSampleRate);
  static float mono[kFrames];
  static int32_t frames[kFrames * 2];
  const uint32_t budget = uint32_t(getCpuFrequencyMhz()) * 1000000u / kSampleRate * kFrames;
  uint32_t worst = 0, windowStart = millis();
  for (;;) {
    SynthTarget target;
    portENTER_CRITICAL(&targetLock);
    target = sharedTarget;
    portEXIT_CRITICAL(&targetLock);

    const uint32_t start = ESP.getCycleCount();
    synth.render(mono, kFrames, target);
    for (int i = 0; i < kFrames; i++) {
      const int32_t v = int32_t(mono[i] * kOutputTrim * 2147483520.0f);
      frames[2 * i] = v;
      frames[2 * i + 1] = v;
    }
    const uint32_t used = ESP.getCycleCount() - start;
    if (used > worst) worst = used;
    if (millis() - windowStart >= 1000) {
      loadPct.store(100.f * worst / budget, std::memory_order_relaxed);
      worst = 0;
      windowStart = millis();
    }
    lastPeak.store(synth.peak(), std::memory_order_relaxed);

    // A full queue drains in ~11 ms; 100 ms without room means the I2S stopped.
    size_t written = 0;
    i2s_write(kPort, frames, sizeof(frames), &written, pdMS_TO_TICKS(100));
    if (written < sizeof(frames)) stalls.fetch_add(1, std::memory_order_relaxed);
  }
}
}  // namespace

void audioBegin() {
  i2s_config_t config = {};
  config.mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = kSampleRate;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 4;
  config.dma_buf_len = kFrames;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;  // underrun plays silence, not a repeated buffer
  config.bits_per_chan = I2S_BITS_PER_CHAN_32BIT;

  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;  // PCM5102 SCK tied to GND: internal PLL from BCK
  pins.bck_io_num = PIN_I2S_BCK;
  pins.ws_io_num = PIN_I2S_LRCK;
  pins.data_out_num = PIN_I2S_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  ESP_ERROR_CHECK(i2s_driver_install(kPort, &config, 0, nullptr));
  ESP_ERROR_CHECK(i2s_set_pin(kPort, &pins));
  i2s_zero_dma_buffer(kPort);
  // Wi-Fi and HTTP run on core 0; audio runs on core 1 above the control loop.
  // DMA backpressure yields regularly to the control/idle tasks.
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, configMAX_PRIORITIES - 5, nullptr, 1);
}

void audioSetTarget(const SynthTarget& target) {
  portENTER_CRITICAL(&targetLock);
  sharedTarget = target;
  portEXIT_CRITICAL(&targetLock);
}

AudioStats audioStats() { return {lastPeak.load(std::memory_order_relaxed), loadPct.load(std::memory_order_relaxed), stalls.load(std::memory_order_relaxed)}; }
