#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum class Tab5RadioCodec : uint8_t { Unknown, Mp3, Aac, Unsupported };

namespace tab5radiocodec {
inline unsigned char lower(unsigned char c) {
  return c >= 'A' && c <= 'Z' ? static_cast<unsigned char>(c + ('a' - 'A')) : c;
}
inline bool equal(const char* first, size_t length, const char* wanted) {
  if (strlen(wanted) != length) return false;
  for (size_t i = 0; i < length; ++i)
    if (lower(static_cast<unsigned char>(first[i])) !=
        lower(static_cast<unsigned char>(wanted[i]))) return false;
  return true;
}
inline Tab5RadioCodec frame(const uint8_t* bytes, size_t length) {
  if (!bytes || length < 4 || bytes[0] != 0xff) return Tab5RadioCodec::Unknown;
  // ADTS has a twelve-bit sync word and layer zero. Validate the fixed
  // header before accepting it; arbitrary 0xff bytes are not a codec probe.
  if (length >= 7 && (bytes[1] & 0xf6u) == 0xf0u) {
    const unsigned frequency = (bytes[2] >> 2) & 15u;
    const unsigned channels = ((bytes[2] & 1u) << 2) | (bytes[3] >> 6);
    const unsigned frameBytes = ((bytes[3] & 3u) << 11) |
                                (unsigned(bytes[4]) << 3) | (bytes[5] >> 5);
    const unsigned headerBytes = (bytes[1] & 1u) ? 7u : 9u;
    if (frequency < 13 && channels && frameBytes >= headerBytes)
      return Tab5RadioCodec::Aac;
  }
  // MPEG Layer III, excluding reserved version/rate and free-format frames.
  if ((bytes[1] & 0xe0u) == 0xe0u && ((bytes[1] >> 3) & 3u) != 1u &&
      ((bytes[1] >> 1) & 3u) == 1u && (bytes[2] >> 4) != 0u &&
      (bytes[2] >> 4) != 15u && ((bytes[2] >> 2) & 3u) != 3u)
    return Tab5RadioCodec::Mp3;
  return Tab5RadioCodec::Unknown;
}
}  // namespace tab5radiocodec

// Catalogue codec names and MIME values share one bounded, allocation-free
// classifier. MIME parameters never influence the codec name.
inline Tab5RadioCodec tab5RadioCodecHint(const char* hint) {
  if (!hint) return Tab5RadioCodec::Unknown;
  while (*hint == ' ' || *hint == '\t') ++hint;
  size_t length = 0;
  while (hint[length] && hint[length] != ';') ++length;
  while (length && (hint[length - 1] == ' ' || hint[length - 1] == '\t')) --length;
  const auto matches = [hint, length](const char* value) {
    return tab5radiocodec::equal(hint, length, value);
  };
  if (matches("mp3") || matches("audio/mpeg") || matches("audio/mp3") ||
      matches("audio/x-mpeg") || matches("audio/mpeg3") || matches("audio/x-mpeg-3"))
    return Tab5RadioCodec::Mp3;
  if (matches("aac") || matches("aac+") || matches("he-aac") || matches("he-aacv2") ||
      matches("audio/aac") || matches("audio/aacp") || matches("audio/x-aac") ||
      matches("audio/aac+")) return Tab5RadioCodec::Aac;
  if (matches("text/html") || matches("application/json") || matches("text/json") ||
      matches("application/ogg") || matches("audio/ogg") || matches("audio/opus") ||
      matches("audio/flac") || matches("audio/x-flac") || matches("audio/wav") ||
      matches("audio/x-wav") || matches("audio/l16") || matches("audio/l24") ||
      matches("audio/mp4") || matches("audio/x-m4a") || matches("audio/webm") ||
      matches("audio/x-scpls") || matches("audio/x-mpegurl") ||
      matches("application/x-mpegurl") || matches("application/vnd.apple.mpegurl") ||
      matches("ogg") || matches("opus") || matches("flac") || matches("wav"))
    return Tab5RadioCodec::Unsupported;
  return Tab5RadioCodec::Unknown;
}

inline Tab5RadioCodec tab5RadioCodecFromUrl(const char* url) {
  if (!url) return Tab5RadioCodec::Unknown;
  // Restrict fallback to the path extension: a query such as ?name=x.aac
  // must not select AAC for an otherwise opaque MP3 endpoint.
  const char* path = url;
  if (const char* scheme = strstr(url, "://")) {
    path = scheme + 3;
    while (*path && *path != '/' && *path != '?' && *path != '#') ++path;
  }
  const char* end = path;
  const char* extension = nullptr;
  while (*end && *end != '?' && *end != '#') {
    if (*end == '/') extension = nullptr;
    else if (*end == '.') extension = end;
    ++end;
  }
  if (!extension) return Tab5RadioCodec::Unknown;
  const size_t length = static_cast<size_t>(end - extension);
  if (tab5radiocodec::equal(extension, length, ".aac")) return Tab5RadioCodec::Aac;
  if (tab5radiocodec::equal(extension, length, ".mp3")) return Tab5RadioCodec::Mp3;
  return Tab5RadioCodec::Unknown;
}

inline Tab5RadioCodec tab5RadioProbeCodec(const uint8_t* bytes, size_t length) {
  if (!bytes) return Tab5RadioCodec::Unknown;
  // Reject recognizable containers/playlists even if the server advertises
  // application/octet-stream. Feeding these to libmad cannot produce sound.
  if ((length >= 4 && (!memcmp(bytes, "OggS", 4) || !memcmp(bytes, "fLaC", 4))) ||
      (length >= 12 && !memcmp(bytes, "RIFF", 4) && !memcmp(bytes + 8, "WAVE", 4)) ||
      (length >= 8 && !memcmp(bytes + 4, "ftyp", 4)) ||
      (length >= 7 && !memcmp(bytes, "#EXTM3U", 7)) ||
      (length >= 10 && !memcmp(bytes, "[playlist]", 10)))
    return Tab5RadioCodec::Unsupported;
  if (length >= 10 && !memcmp(bytes, "ID3", 3)) {
    if ((bytes[6] | bytes[7] | bytes[8] | bytes[9]) & 0x80u)
      return Tab5RadioCodec::Unknown;
    const size_t tagBytes = (size_t(bytes[6]) << 21) | (size_t(bytes[7]) << 14) |
                            (size_t(bytes[8]) << 7) | size_t(bytes[9]);
    const size_t offset = 10u + tagBytes + ((bytes[5] & 0x10u) ? 10u : 0u);
    // ID3 by itself is not proof of MP3: keep MIME/catalogue hints usable
    // for AAC streams whose leading metadata exceeds this small probe.
    if (offset > length) return Tab5RadioCodec::Unknown;
    return tab5radiocodec::frame(bytes + offset, length - offset);
  }
  return tab5radiocodec::frame(bytes, length);
}

inline Tab5RadioCodec tab5RadioSelectCodec(const uint8_t* prefix, size_t length,
    Tab5RadioCodec mime, Tab5RadioCodec catalogue, const char* url) {
  if (mime == Tab5RadioCodec::Unsupported) return mime;
  const auto probe = tab5RadioProbeCodec(prefix, length);
  if (probe != Tab5RadioCodec::Unknown) return probe;
  if (mime != Tab5RadioCodec::Unknown) return mime;
  if (catalogue != Tab5RadioCodec::Unknown) return catalogue;
  const auto extension = tab5RadioCodecFromUrl(url);
  return extension == Tab5RadioCodec::Unknown ? Tab5RadioCodec::Mp3 : extension;
}
