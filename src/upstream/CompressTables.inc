// CompressDialog.cpp
// Copyright (C) 1999-2026 Igor Pavlov. GNU LGPL-2.1-or-later.
// Generated from official 7-Zip 26.03 by scripts/import-compression-math.py.
// Original arithmetic bodies retained. Thread/solid bodies omit Win32 combo
// plumbing; signatures return results through the adapter in CompressionMath.cpp.
// Original source SHA-256: 53d673d78e7936880481be1017c37ca95a1e4b64419d74d32035ce703d06c7d4

static const UInt32 g_Levels[] =
{
  4050,
  4051,
  0,
  4052,
  0,
  4053,
  0,
  4054,
  0,
  4055
};

enum EMethodID
{
  kCopy,
  kLZMA,
  kLZMA2,
  kPPMd,
  kBZip2,
  kDeflate,
  kDeflate64,
  kPPMdZip,
  // kZSTD,
  kSha256,
  kSha1,
  kCrc32,
  kCrc64,
  kGnu,
  kPosix
};

static LPCSTR const kMethodsNames[] =
{
    "Copy"
  , "LZMA"
  , "LZMA2"
  , "PPMd"
  , "BZip2"
  , "Deflate"
  , "Deflate64"
  , "PPMd"
  // , "ZSTD"
  , "SHA256"
  , "SHA1"
  , "CRC32"
  , "CRC64"
  , "GNU"
  , "POSIX"
};

static const EMethodID g_7zMethods[] =
{
  kLZMA2,
  kLZMA,
  kPPMd,
  kBZip2
  , kDeflate
  , kDeflate64
  // , kZSTD
  , kCopy
};

static const EMethodID g_7zSfxMethods[] =
{
  kCopy,
  kLZMA,
  kLZMA2,
  kPPMd
};

static const EMethodID g_ZipMethods[] =
{
  kDeflate,
  kDeflate64,
  kBZip2,
  kLZMA,
  kPPMdZip
  // , kZSTD
};

static const EMethodID g_GZipMethods[] =
{
  kDeflate
};

static const EMethodID g_BZip2Methods[] =
{
  kBZip2
};

static const EMethodID g_XzMethods[] =
{
  kLZMA2
};

/*
static const EMethodID g_ZstdMethods[] =
{
  kZSTD
};
*/

/*
static const EMethodID g_SwfcMethods[] =
{
  kDeflate
  // kLZMA
};
*/

static const EMethodID g_TarMethods[] =
{
  kGnu,
  kPosix
};

static const EMethodID g_HashMethods[] =
{
    kSha256
  , kSha1
  // , kCrc32
  // , kCrc64
};

static const UInt32 kFF_Filter      = 1 << 0;
static const UInt32 kFF_Solid       = 1 << 1;
static const UInt32 kFF_MultiThread = 1 << 2;
static const UInt32 kFF_Encrypt     = 1 << 3;
static const UInt32 kFF_EncryptFileNames  = 1 << 4;
static const UInt32 kFF_MemUse      = 1 << 5;
static const UInt32 kFF_SFX         = 1 << 6;

/*
static const UInt32 kFF_Time_Win  = 1 << 10;
static const UInt32 kFF_Time_Unix = 1 << 11;
static const UInt32 kFF_Time_DOS  = 1 << 12;
static const UInt32 kFF_Time_1ns  = 1 << 13;
*/

struct CFormatInfo
{
  LPCSTR Name;
  UInt32 LevelsMask;
  unsigned NumMethods;
  const EMethodID *MethodIDs;

  UInt32 Flags;

  bool Filter_() const { return (Flags & kFF_Filter) != 0; }
  bool Solid_() const { return (Flags & kFF_Solid) != 0; }
  bool MultiThread_() const { return (Flags & kFF_MultiThread) != 0; }
  bool Encrypt_() const { return (Flags & kFF_Encrypt) != 0; }
  bool EncryptFileNames_() const { return (Flags & kFF_EncryptFileNames) != 0; }
  bool MemUse_() const { return (Flags & kFF_MemUse) != 0; }
  bool SFX_() const { return (Flags & kFF_SFX) != 0; }
};

#define METHODS_PAIR(x) Z7_ARRAY_SIZE(x), x

static const CFormatInfo g_Formats[] =
{
  {
    "",
    // (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    ((UInt32)1 << 10) - 1,
    // (UInt32)(Int32)-1,
    0, NULL,
    kFF_MultiThread | kFF_MemUse
  },
  {
    "7z",
    // (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    (1 << 10) - 1,
    METHODS_PAIR(g_7zMethods),
    kFF_Filter | kFF_Solid | kFF_MultiThread | kFF_Encrypt |
    kFF_EncryptFileNames | kFF_MemUse | kFF_SFX
    // | kFF_Time_Win
  },
  {
    "Zip",
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_ZipMethods),
    kFF_MultiThread | kFF_Encrypt | kFF_MemUse
    // | kFF_Time_Win | kFF_Time_Unix | kFF_Time_DOS
  },
  {
    "GZip",
    (1 << 1) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_GZipMethods),
    kFF_MemUse
    // | kFF_Time_Unix
  },
  {
    "BZip2",
    (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_BZip2Methods),
    kFF_MultiThread | kFF_MemUse
  },
  {
    "xz",
    // (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    (1 << 10) - 1 - (1 << 0), // store (1 << 0) is not supported
    METHODS_PAIR(g_XzMethods),
    kFF_Solid | kFF_MultiThread | kFF_MemUse
  },
  /*
  {
    "zstd",
    // (1 << (MY_ZSTD_LEVEL_MAX + 1)) - 1,
    (1 << (9 + 1)) - 1,
    METHODS_PAIR(g_ZstdMethods),
    // kFF_Solid |
    kFF_MultiThread
    | kFF_MemUse
  },
  */
/*
  {
    "Swfc",
    (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_SwfcMethods),
    0
  },
*/
  {
    "Tar",
    (1 << 0),
    METHODS_PAIR(g_TarMethods),
    0
    // kFF_Time_Unix | kFF_Time_Win // | kFF_Time_1ns
  },
  {
    "wim",
    (1 << 0),
    0, NULL,
    0
    // | kFF_Time_Win
  },
  {
    "Hash",
    (0 << 0),
    METHODS_PAIR(g_HashMethods),
    0
  }
};

