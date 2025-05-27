#pragma once

//
// KS pin definitions for audio driver
// These definitions are typically available in the Windows DDK but may be missing in some versions
//

#ifndef KS_PIN_DEFS_DEFINED
#define KS_PIN_DEFS_DEFINED

// Render pin definitions
#define KSPIN_WAVE_RENDER_SOURCE              0       // Source pin for rendering
#define KSPIN_WAVE_RENDER_SINK_SYSTEM         1       // Sink pin for system audio
#define KSPIN_WAVE_RENDER_SINK_OFFLOAD        2       // Sink pin for offloaded audio
#define KSPIN_WAVE_RENDER_SINK_LOOPBACK       3       // Sink pin for loopback

// Additional render pins
#define KSPIN_WAVE_RENDER2_SOURCE             0       // Second render source
#define KSPIN_WAVE_RENDER2_SINK_SYSTEM        1       // Second render system sink
#define KSPIN_WAVE_RENDER2_SINK_LOOPBACK      2       // Second render loopback sink

#define KSPIN_WAVE_RENDER3_SOURCE             0       // Third render source
#define KSPIN_WAVE_RENDER3_SINK_SYSTEM        1       // Third render system sink

// Capture pin definitions
#define KSPIN_WAVE_CAPTURE_SOURCE             0       // Source pin for capture
#define KSPIN_WAVE_CAPTURE_SINK               1       // Sink pin for capture

// Keyword capture pins
#define KSPIN_WAVEIN_KEYWORD                  0       // Keyword detection pin

// Bidirectional pin
#define KSPIN_WAVE_BIDI                       0       // Bidirectional audio pin

// Audio node definitions
#define KSNODE_WAVE_ADC                       0       // ADC node
#define KSNODE_WAVE_DAC                       0       // DAC node
#define KSNODE_WAVE_SUM                       1       // Sum node

// Wave bridge pin
#define KSPIN_WAVE_BRIDGE                     0       // Bridge pin

// Define KSPROPSETID_AudioEffectsDiscovery if not already defined
#ifndef KSPROPSETID_AudioEffectsDiscovery_DEFINED
#define KSPROPSETID_AudioEffectsDiscovery_DEFINED
DEFINE_GUID(KSPROPSETID_AudioEffectsDiscovery, 
            0xb217a72, 0x16b8, 0x4a6d, 0x84, 0x21, 0x70, 0x5e, 0xf3, 0x19, 0x7, 0x2f);
#endif

// Define KSPROPERTY_AUDIOEFFECTSDISCOVERY_EFFECTSLIST if not already defined
#define KSPROPERTY_AUDIOEFFECTSDISCOVERY_EFFECTSLIST 0

#endif // KS_PIN_DEFS_DEFINED
