#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <cstdint> // For uint32_t, uint16_t, int16_t

// Basic WAV file writer
// Note: Only supports 16-bit PCM mono/stereo for simplicity in this example.
// For more advanced features (e.g., floating point, multi-channel beyond stereo),
// a more robust library or implementation would be needed.

namespace WavWriter {

#pragma pack(push, 1) // Exact byte layout
    struct WavHeader {
        char     RIFF[4] = {'R', 'I', 'F', 'F'};
        uint32_t chunkSize;                     // Size of the entire file in bytes minus 8 bytes
        char     WAVE[4] = {'W', 'A', 'V', 'E'};
        char     fmt[4] = {'f', 'm', 't', ' '}; // Fmt subchunk
        uint32_t fmtChunkSize = 16;             // Size of the fmt subchunk (16 for PCM)
        uint16_t audioFormat = 1;               // Audio format 1=PCM, 6=mulaw, 7=alaw, 257=IBM Mu-Law, ...
        uint16_t numChannels;                   // Number of channels 1=Mono 2=Stereo
        uint32_t sampleRate;                    // Sampling Frequency in Hz
        uint32_t byteRate;                      // bytes per second == SampleRate * NumChannels * BitsPerSample/8
        uint16_t blockAlign;                    // NumChannels * BitsPerSample/8
        uint16_t bitsPerSample;                 // Number of bits per sample
        char     data[4] = {'d', 'a', 't', 'a'}; // "data" subchunk
        uint32_t dataChunkSize;                 // NumSamples * NumChannels * BitsPerSample/8 - size of the next chunk that will be read
    };
#pragma pack(pop)

    // Writes audio data to a WAV file.
    // audioData: vector of interleaved 16-bit PCM samples.
    // filename: path to the output WAV file.
    // numChannels: 1 for mono, 2 for stereo.
    // sampleRate: e.g., 44100, 48000.
    // bitsPerSample: must be 16 for this simplified writer.
    bool writeWavFile(const std::string& filename, 
                      const std::vector<int16_t>& audioData, 
                      uint16_t numChannels, 
                      uint32_t sampleRate, 
                      uint16_t bitsPerSample) {
        
        if (bitsPerSample != 16) {
            // This simple writer only supports 16-bit PCM.
            return false; 
        }
        if (numChannels == 0 || numChannels > 2) {
            // This simple writer only supports mono or stereo.
            // For 16-channel driver, would need to adapt or use a different writer.
            return false;
        }

        std::ofstream outFile(filename, std::ios::binary | std::ios::trunc);
        if (!outFile.is_open()) {
            return false;
        }

        WavHeader header;

        header.numChannels = numChannels;
        header.sampleRate = sampleRate;
        header.bitsPerSample = bitsPerSample;
        header.blockAlign = (numChannels * bitsPerSample) / 8;
        header.byteRate = sampleRate * header.blockAlign;
        
        // dataChunkSize is the total size of the audio data in bytes
        header.dataChunkSize = static_cast<uint32_t>(audioData.size() * sizeof(int16_t));
        // chunkSize is the size of the entire file minus the RIFF descriptor and chunkSize field
        header.chunkSize = 36 + header.dataChunkSize; // 36 is the size of the header part before dataChunkSize

        outFile.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));
        
        // Write the audio data
        if (!audioData.empty()) {
            outFile.write(reinterpret_cast<const char*>(audioData.data()), header.dataChunkSize);
        }
        
        bool success = outFile.good();
        outFile.close();
        return success;
    }

} // namespace WavWriter
