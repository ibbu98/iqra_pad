#include "audio_player.h"
#include "sd_card.h"

#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <AudioFileSource.h>
#include <freertos/queue.h>

// ── SdFat-backed AudioFileSource ─────────────────────────────────────────────
class FileSourceSdFat : public AudioFileSource {
public:
  bool open(const char* path) {
    if (_f.isOpen()) _f.close();
    return _f.open(path, O_RDONLY);
  }
  uint32_t read(void* buf, uint32_t len) override { return _f.read(buf, len); }
  bool seek(int32_t pos, int dir) override {
    switch (dir) {
      case 0:  return _f.seekSet(pos);
      case 1:  return _f.seekCur(pos);
      default: return _f.seekEnd(pos);
    }
  }
  bool     close()   override { _f.close(); return true; }
  bool     isOpen()  override { return _f.isOpen(); }
  uint32_t getSize() override { return _f.fileSize(); }
  uint32_t getPos()  override { return _f.curPosition(); }
private:
  File32 _f;
};

// ── Module state ──────────────────────────────────────────────────────────────
static FileSourceSdFat*   _src      = nullptr;
static AudioGeneratorMP3* _mp3      = nullptr;
static AudioOutputI2S*    _out      = nullptr;
static volatile bool      _playing  = false;
static volatile bool      _finished = false;
static float              _volume   = 0.5f;

// ── Command queue (Core 1 → Core 0) ──────────────────────────────────────────
// All _mp3 / _out operations happen on Core 0 only.
// Core 1 never calls begin() / stop() / loop() — it only posts commands here.
struct AudioCmd {
  char path[200];   // empty string = stop
};
static QueueHandle_t _cmdQ = nullptr;

// Called from Core 0 audio task — executes any pending play/stop command.
static void handleCmd()
{
  AudioCmd cmd;
  if (xQueueReceive(_cmdQ, &cmd, 0) != pdTRUE) return;

  // Stop whatever was running
  if (_mp3->isRunning()) _mp3->stop();
  if (_src->isOpen())    _src->close();
  _playing = false;

  if (cmd.path[0] == '\0') return;  // stop-only command

  Serial.printf("[AUDIO] play %s\n", cmd.path);

  if (!_src->open(cmd.path)) {
    Serial.printf("[AUDIO] FAIL — cannot open %s\n", cmd.path);
    return;
  }
  if (!_mp3->begin(_src, _out)) {
    Serial.println("[AUDIO] FAIL — mp3->begin() failed");
    _src->close();
    return;
  }
  _finished = false;
  _playing  = true;
}

// ── Public API ────────────────────────────────────────────────────────────────
void audioInit()
{
  _cmdQ = xQueueCreate(1, sizeof(AudioCmd));

  _out = new AudioOutputI2S();
  _out->SetPinout(I2S_BCLK_PIN, I2S_LRCK_PIN, I2S_DOUT_PIN);
  _out->SetGain(_volume * 2.0f);

  _src = new FileSourceSdFat();
  _mp3 = new AudioGeneratorMP3();

  Serial.printf("[AUDIO] init  BCLK=%d  LRC=%d  DOUT=%d\n",
                I2S_BCLK_PIN, I2S_LRCK_PIN, I2S_DOUT_PIN);
}

void audioPlay(const String& folder, const String& fileNoExt)
{
  AudioCmd cmd;
  snprintf(cmd.path, sizeof(cmd.path), "/%s/%s.mp3",
           folder.c_str(), fileNoExt.c_str());
  xQueueOverwrite(_cmdQ, &cmd);   // Core 0 will call begin()
}

void audioStop()
{
  AudioCmd cmd;
  cmd.path[0] = '\0';             // empty = stop only
  xQueueOverwrite(_cmdQ, &cmd);
}

bool audioIsPlaying()    { return _playing; }

bool audioJustFinished() {
  if (_finished) { _finished = false; return true; }
  return false;
}

void audioSetVolume(float v) {
  v = constrain(v, 0.0f, 1.0f);
  _volume = v;
  if (_out) _out->SetGain(v * 2.0f);
}

// Called from Core 0 audio task every 1 ms.
void audioLoop()
{
  handleCmd();   // execute any pending play/stop command first

  if (!_playing || !_mp3) return;

  if (_mp3->isRunning()) {
    if (!_mp3->loop()) {
      Serial.println("[AUDIO] track ended");
      _mp3->stop();
      _playing  = false;
      _finished = true;
    }
  } else {
    _playing  = false;
    _finished = true;
  }
}
