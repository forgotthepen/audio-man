#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <optional>
#include <vector>
#include <thread>
#include <ctime>
#include <sstream>
#include <set>
#include <map>
#include <algorithm>
#include <functional>
#include <iterator>
#include <future>
#include <cstdint> // uintxx_t
#include <cstring> // std::memcpy


#include "audio_man/audio_man.hpp"


// WAV header
#pragma pack(push, 1)
struct WavHeader_t
{
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t chunkSize;             // Total size minus 8 bytes (above 4 bytes + this field)
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t fmtChunkSize = 16;     // PCM header size
    uint16_t audioFormat = 1;       // PCM = 1
    uint16_t numChannels;           // 1 = Mono, 2 = Stereo
    uint32_t sampleRate;            // e.g., 44100
    uint32_t byteRate;              // = sampleRate * numChannels * bytesPerSample
    uint16_t blockAlign;            // = numChannels * bytesPerSample
    uint16_t bitsPerSample;         // e.g., 16
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t dataSize;              // Number of bytes in data section
};
#pragma pack(pop)

std::vector<char> pcm_to_wav(const std::vector<char> &pcm_data, uint32_t sample_rate, uint16_t channels_count, uint16_t bits_per_sample)
{
  uint32_t dataSize = static_cast<uint32_t>(pcm_data.size());
  uint32_t chunkSize = 36 + dataSize;

  WavHeader_t header{};
  header.chunkSize = chunkSize;
  header.numChannels = channels_count;
  header.sampleRate = sample_rate;
  header.bitsPerSample = bits_per_sample;
  header.byteRate = sample_rate * channels_count * (bits_per_sample / 8);
  header.blockAlign = channels_count * (bits_per_sample / 8);
  header.dataSize = dataSize;

  // concat header and pcm data
  std::vector<char> wavData(sizeof(WavHeader_t) + dataSize);
  std::memcpy(wavData.data(), &header, sizeof(WavHeader_t));
  std::memcpy(wavData.data() + sizeof(WavHeader_t), pcm_data.data(), dataSize);

  return wavData;
}



int main(int argc, char** argv)
{
  auto amn = AudioMan();
  if (!amn.InitPlayback()) {
    std::cerr << "failed to init playback device\n";
    return 1;
  }

 std::chrono::high_resolution_clock::time_point tttt[5]{};
  {
    if (amn.StartRecording(48000, 2, RecordingFormat_t::Signed16)) {
      std::cout << "started mic loopback!" << std::endl;

      auto tt1 = std::chrono::high_resolution_clock::now();
      while (amn.IsRecording()) {
        auto chunks_size = amn.SizeUnreadRecording();
        if (!chunks_size) {
          // std::cout << "silence! (empty sz)" << std::endl;
          continue;
        }
auto t1 = std::chrono::high_resolution_clock::now();
        auto chunks = amn.GetUnreadRecording();
        if (!chunks.empty()) {
          auto deco = amn.DecodeRecordingChunks(chunks);
          auto wav = pcm_to_wav(deco, 48000, 2, 16);
          amn.SubmitAudio(wav);
auto t2 = std::chrono::high_resolution_clock::now();
          std::cout << "## data ##" << std::endl;
auto dd = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
std::cout << dd << " ";
          // std::this_thread::sleep_for(std::chrono::milliseconds(2));
std::cout << std::endl;


          // std::this_thread::sleep_for(std::chrono::milliseconds(200));
        } else {
          std::cout << "silence! (no buffer)" << std::endl;
        }
        
        auto tt2 = std::chrono::high_resolution_clock::now();
        auto ddd = std::chrono::duration_cast<std::chrono::seconds>(tt2 - tt1).count();
        if (ddd > 555) {
          break;
        }
      }
      
      amn.StopRecording();
      std::cout << "stopped mic loopback!" << std::endl;
    }
  }

  {
    if (amn.StartRecording(44100, 1, RecordingFormat_t::Signed16)) {
      std::cout << "started mic!" << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(5));
      amn.StopRecording();
      std::cout << "stopped mic!" << std::endl;

      auto chunks = amn.GetUnreadRecording();
      auto deco = amn.DecodeRecordingChunks(chunks);
      auto dd = pcm_to_wav(deco, 44100, 1, 16);

      auto resrec = amn.SubmitAudio(dd);
      std::cout << "recording playback=" << resrec.Wait() << std::endl;
    }
  }


  if (argc < 2) {
    std::cerr << "no input file\n";
    return -1;
  }
  
  std::vector<char> data{};
  auto fdata = std::ifstream(argv[1], std::ios::in | std::ios::binary);
  fdata.seekg(0, std::ios::end);
  data.resize(fdata.tellg());
  fdata.seekg(0, std::ios::beg);
  fdata.read(&data[0], data.size());
  fdata.close();

  auto res = amn.SubmitAudio(data);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  auto res2 = amn.SubmitAudio(data);

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  amn.CancelAllPlayback();

  auto b1 = res.Wait();
  auto b2 = res2.Wait();

  std::cout << "b1=" << b1 << " b2=" << b2 << std::endl;
  std::cout << "b1=" << res.FutureResult().get() << " b2=" << res2.FutureResult().get() << std::endl;
  res.Cancel();
  std::cout << "ran!" << std::endl;


  std::string asd{};
  std::cin >> asd;

  return 0;
}
