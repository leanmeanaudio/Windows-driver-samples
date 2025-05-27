#include <wdm.h>
#include <portcls.h>
#include "ksdatarange_compat.h"  // Include our compatibility header
#include "lamaloopbackcommon.h"
#include "lamaloopbackrender.h"

// Modified version of DataRangeIntersection that uses safe functions
// for accessing KSDATARANGE_AUDIO fields
NTSTATUS CMiniportTopologyLamaLoopbackRender::DataRangeIntersection
(
    _In_        ULONG           PinId,
    _In_        PKSDATARANGE    DataRange,          // Client's proposed data range
    _In_        PKSDATARANGE    MatchingDataRange,  // Pin's data range (PcmAudioDataRange)
    _In_        ULONG           OutputBufferLength,
    _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength)
                PVOID           ResultantFormat,    // Buffer for the resulting format
    _Out_       PULONG          ResultantFormatLength
)
{
    PAGED_CODE();
    DbgPrint("Entered CMiniportTopologyLamaLoopbackRender::DataRangeIntersection PinId=%u\n", PinId);
    UNREFERENCED_PARAMETER(MatchingDataRange);

    if (!DataRange || !ResultantFormatLength) {
        return STATUS_INVALID_PARAMETER;
    }
    
    *ResultantFormatLength = 0; // Default to zero

    // Validate basic format type (Audio, PCM, WaveFormatEx)
    if (!IsEqualGUIDAligned(DataRange->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) ||
        !IsEqualGUIDAligned(DataRange->SubFormat, KSDATAFORMAT_SUBTYPE_PCM) ||
        !IsEqualGUIDAligned(DataRange->Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX))
    {
        DbgPrint("DataRangeIntersection (Render): No match due to Major/Sub/Specifier GUIDs.\n");
        return STATUS_NO_MATCH;
    }

    // Get safe channel information using our helper function
    ULONG clientMinChannels = 1;
    ULONG clientMaxChannels = 2;
    GetSafeChannelInfo(DataRange, &clientMinChannels, &clientMaxChannels);
    ULONG resultChannels;

    if (clientMaxChannels == 0 || clientMaxChannels > MAX_CHANNELS_PCM) {
        clientMaxChannels = MAX_CHANNELS_PCM;
    }
   
    if (clientMinChannels > clientMaxChannels) {
        DbgPrint("DataRangeIntersection (Render): Client min channels %u > client max channels %u. No match.\n", 
            clientMinChannels, clientMaxChannels);
        return STATUS_NO_MATCH; 
    }
    
    if (clientMaxChannels >= MIN_CHANNELS_PCM && clientMaxChannels <= MAX_CHANNELS_PCM) {
        resultChannels = clientMaxChannels;
    } else if (clientMinChannels >= MIN_CHANNELS_PCM && clientMinChannels <= MAX_CHANNELS_PCM) {
        resultChannels = clientMinChannels;
    } else {
        DbgPrint("DataRangeIntersection (Render): Client channel range [%u, %u] is outside device capabilities [%u, %u]. No match.\n",
            clientMinChannels, clientMaxChannels, MIN_CHANNELS_PCM, MAX_CHANNELS_PCM);
        return STATUS_NO_MATCH;
    }
   
    if (resultChannels < MIN_CHANNELS_PCM || resultChannels > MAX_CHANNELS_PCM) {
        DbgPrint("DataRangeIntersection (Render): Selected resultChannels %u is outside device capabilities [%u, %u]. No match.\n",
            resultChannels, MIN_CHANNELS_PCM, MAX_CHANNELS_PCM);
        return STATUS_NO_MATCH;
    }

    // Sample Rate and Bits Per Sample logic
    ULONG resultBitsPerSample = g_CurrentGlobalBitsPerSample;
    ULONG resultSampleRate = g_CurrentGlobalSampleRate;      

    // Get safe bit depth and sample rate information using our helper functions
    ULONG minBitsPerSample, maxBitsPerSample, minSampleRate, maxSampleRate;
    GetSafeBitDepthInfo(DataRange, &minBitsPerSample, &maxBitsPerSample);
    GetSafeSampleRateInfo(DataRange, &minSampleRate, &maxSampleRate);

    // Check compatibility with safe ranges
    if (resultBitsPerSample < minBitsPerSample || resultBitsPerSample > maxBitsPerSample) {
        DbgPrint("DataRangeIntersection (Render): Global BPS %u not in client range [%u, %u]\n", 
            resultBitsPerSample, minBitsPerSample, maxBitsPerSample);
        return STATUS_NO_MATCH;
    }

    if (resultSampleRate < minSampleRate || resultSampleRate > maxSampleRate) {
        DbgPrint("DataRangeIntersection (Render): Global SR %u not in client range [%u, %u]\n", 
            resultSampleRate, minSampleRate, maxSampleRate);
        return STATUS_NO_MATCH;
    }

    if (resultBitsPerSample < MIN_BITS_PER_SAMPLE_PCM || resultBitsPerSample > MAX_BITS_PER_SAMPLE_PCM ||
        resultSampleRate < MIN_SAMPLE_RATE_PCM || resultSampleRate > MAX_SAMPLE_RATE_PCM) {
        DbgPrint("DataRangeIntersection (Render): Resulting format SR/BPS (%uHz, %ubit) outside device capabilities.\n",
            resultSampleRate, resultBitsPerSample);
        return STATUS_NO_MATCH;
    }

    if (!ResultantFormat) 
    {
        *ResultantFormatLength = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
        return STATUS_BUFFER_OVERFLOW; 
    }

    if (OutputBufferLength < sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE))
    {
        DbgPrint("DataRangeIntersection: Output buffer too small. Needed %u, Got %u\n", 
            (ULONG)sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE), OutputBufferLength);
        return STATUS_BUFFER_TOO_SMALL;
    }

    PKSDATAFORMAT_WAVEFORMATEXTENSIBLE pResFormatWfx = (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE)ResultantFormat;
    
    pResFormatWfx->DataFormat.FormatSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    pResFormatWfx->DataFormat.Flags = 0;
    pResFormatWfx->DataFormat.SampleSize = (resultBitsPerSample / 8) * resultChannels; 
    pResFormatWfx->DataFormat.Reserved = 0;
    pResFormatWfx->DataFormat.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
    pResFormatWfx->DataFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    pResFormatWfx->DataFormat.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;

    WAVEFORMATEXTENSIBLE *pWfx = &pResFormatWfx->WaveFormatExt;
    pWfx->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    pWfx->Format.nChannels = (WORD)resultChannels;
    pWfx->Format.nSamplesPerSec = resultSampleRate;
    pWfx->Format.wBitsPerSample = (WORD)resultBitsPerSample;
    pWfx->Format.nBlockAlign = (WORD)((pWfx->Format.nChannels * pWfx->Format.wBitsPerSample) / 8);
    pWfx->Format.nAvgBytesPerSec = pWfx->Format.nSamplesPerSec * pWfx->Format.nBlockAlign;
    pWfx->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    
    pWfx->Samples.wValidBitsPerSample = pWfx->Format.wBitsPerSample;
    
    if (pWfx->Format.nChannels == 1) {
        pWfx->dwChannelMask = KSAUDIO_SPEAKER_MONO;
    } else if (pWfx->Format.nChannels == 2) {
        pWfx->dwChannelMask = KSAUDIO_SPEAKER_STEREO;
    } else {
        pWfx->dwChannelMask = 0; 
    }
    pWfx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    *ResultantFormatLength = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    DbgPrint("DataRangeIntersection (Render): Success. Format: %uHz, %uch, %ubit\n", 
        pWfx->Format.nSamplesPerSec, pWfx->Format.nChannels, pWfx->Format.wBitsPerSample);
    
    return STATUS_SUCCESS;
}

// Modified version of DataRangeIntersection for WaveRT renderer
NTSTATUS CMiniportWaveRTLamaLoopbackRender::DataRangeIntersection
(
    _In_        ULONG           PinId,
    _In_        PKSDATARANGE    DataRange,
    _In_        PKSDATARANGE    MatchingDataRange,
    _In_        ULONG           OutputBufferLength,
    _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength)
                PVOID           ResultantFormat,
    _Out_       PULONG          ResultantFormatLength
)
{
    PAGED_CODE();
    DbgPrint("Entered CMiniportWaveRTLamaLoopbackRender::DataRangeIntersection PinId=%u\n", PinId);
    UNREFERENCED_PARAMETER(MatchingDataRange);

    if (!DataRange || !ResultantFormatLength) {
        return STATUS_INVALID_PARAMETER;
    }
    
    *ResultantFormatLength = 0; // Default to zero

    // Validate basic format type (Audio, PCM, WaveFormatEx)
    if (!IsEqualGUIDAligned(DataRange->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) ||
        !IsEqualGUIDAligned(DataRange->SubFormat, KSDATAFORMAT_SUBTYPE_PCM) ||
        !IsEqualGUIDAligned(DataRange->Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX))
    {
        DbgPrint("DataRangeIntersection (Render): No match due to Major/Sub/Specifier GUIDs.\n");
        return STATUS_NO_MATCH;
    }

    // Get safe channel information using our helper function
    ULONG clientMinChannels = 1;
    ULONG clientMaxChannels = 2;
    GetSafeChannelInfo(DataRange, &clientMinChannels, &clientMaxChannels);
    ULONG resultChannels;

    if (clientMaxChannels == 0 || clientMaxChannels > MAX_CHANNELS_PCM) {
        clientMaxChannels = MAX_CHANNELS_PCM;
    }
   
    if (clientMinChannels > clientMaxChannels) {
        DbgPrint("DataRangeIntersection (Render): Client min channels %u > client max channels %u. No match.\n", 
            clientMinChannels, clientMaxChannels);
        return STATUS_NO_MATCH; 
    }
    
    if (clientMaxChannels >= MIN_CHANNELS_PCM && clientMaxChannels <= MAX_CHANNELS_PCM) {
        resultChannels = clientMaxChannels;
    } else if (clientMinChannels >= MIN_CHANNELS_PCM && clientMinChannels <= MAX_CHANNELS_PCM) {
        resultChannels = clientMinChannels;
    } else {
        DbgPrint("DataRangeIntersection (Render): Client channel range [%u, %u] is outside device capabilities [%u, %u]. No match.\n",
            clientMinChannels, clientMaxChannels, MIN_CHANNELS_PCM, MAX_CHANNELS_PCM);
        return STATUS_NO_MATCH;
    }
   
    if (resultChannels < MIN_CHANNELS_PCM || resultChannels > MAX_CHANNELS_PCM) {
        DbgPrint("DataRangeIntersection (Render): Selected resultChannels %u is outside device capabilities [%u, %u]. No match.\n",
            resultChannels, MIN_CHANNELS_PCM, MAX_CHANNELS_PCM);
        return STATUS_NO_MATCH;
    }

    // Sample Rate and Bits Per Sample logic
    ULONG resultBitsPerSample = g_CurrentGlobalBitsPerSample;
    ULONG resultSampleRate = g_CurrentGlobalSampleRate;      

    // Get safe bit depth and sample rate information using our helper functions
    ULONG minBitsPerSample, maxBitsPerSample, minSampleRate, maxSampleRate;
    GetSafeBitDepthInfo(DataRange, &minBitsPerSample, &maxBitsPerSample);
    GetSafeSampleRateInfo(DataRange, &minSampleRate, &maxSampleRate);

    // Check compatibility with safe ranges
    if (resultBitsPerSample < minBitsPerSample || resultBitsPerSample > maxBitsPerSample) {
        DbgPrint("DataRangeIntersection (Render): Global BPS %u not in client range [%u, %u]\n", 
            resultBitsPerSample, minBitsPerSample, maxBitsPerSample);
        return STATUS_NO_MATCH;
    }

    if (resultSampleRate < minSampleRate || resultSampleRate > maxSampleRate) {
        DbgPrint("DataRangeIntersection (Render): Global SR %u not in client range [%u, %u]\n", 
            resultSampleRate, minSampleRate, maxSampleRate);
        return STATUS_NO_MATCH;
    }

    if (resultBitsPerSample < MIN_BITS_PER_SAMPLE_PCM || resultBitsPerSample > MAX_BITS_PER_SAMPLE_PCM ||
        resultSampleRate < MIN_SAMPLE_RATE_PCM || resultSampleRate > MAX_SAMPLE_RATE_PCM) {
        DbgPrint("DataRangeIntersection (Render): Resulting format SR/BPS (%uHz, %ubit) outside device capabilities.\n",
            resultSampleRate, resultBitsPerSample);
        return STATUS_NO_MATCH;
    }

    if (!ResultantFormat) 
    {
        *ResultantFormatLength = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
        return STATUS_BUFFER_OVERFLOW; 
    }

    if (OutputBufferLength < sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE))
    {
        DbgPrint("DataRangeIntersection: Output buffer too small. Needed %u, Got %u\n", 
            (ULONG)sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE), OutputBufferLength);
        return STATUS_BUFFER_TOO_SMALL;
    }

    PKSDATAFORMAT_WAVEFORMATEXTENSIBLE pResFormatWfx = (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE)ResultantFormat;
    
    pResFormatWfx->DataFormat.FormatSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    pResFormatWfx->DataFormat.Flags = 0;
    pResFormatWfx->DataFormat.SampleSize = (resultBitsPerSample / 8) * resultChannels; 
    pResFormatWfx->DataFormat.Reserved = 0;
    pResFormatWfx->DataFormat.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
    pResFormatWfx->DataFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    pResFormatWfx->DataFormat.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;

    WAVEFORMATEXTENSIBLE *pWfx = &pResFormatWfx->WaveFormatExt;
    pWfx->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    pWfx->Format.nChannels = (WORD)resultChannels;
    pWfx->Format.nSamplesPerSec = resultSampleRate;
    pWfx->Format.wBitsPerSample = (WORD)resultBitsPerSample;
    pWfx->Format.nBlockAlign = (WORD)((pWfx->Format.nChannels * pWfx->Format.wBitsPerSample) / 8);
    pWfx->Format.nAvgBytesPerSec = pWfx->Format.nSamplesPerSec * pWfx->Format.nBlockAlign;
    pWfx->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    
    pWfx->Samples.wValidBitsPerSample = pWfx->Format.wBitsPerSample;
    
    if (pWfx->Format.nChannels == 1) {
        pWfx->dwChannelMask = KSAUDIO_SPEAKER_MONO;
    } else if (pWfx->Format.nChannels == 2) {
        pWfx->dwChannelMask = KSAUDIO_SPEAKER_STEREO;
    } else {
        pWfx->dwChannelMask = 0; 
    }
    pWfx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    *ResultantFormatLength = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    DbgPrint("DataRangeIntersection (Render): Success. Format: %uHz, %uch, %ubit\n", 
        pWfx->Format.nSamplesPerSec, pWfx->Format.nChannels, pWfx->Format.wBitsPerSample);
    
    return STATUS_SUCCESS;
}
