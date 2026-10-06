// CompressDialog.cpp
// Copyright (C) 1999-2026 Igor Pavlov. GNU LGPL-2.1-or-later.
// Generated from official 7-Zip 26.03 by scripts/import-compression-math.py.
// Original arithmetic bodies retained. Thread/solid bodies omit Win32 combo
// plumbing; signatures return results through the adapter in CompressionMath.cpp.
// Original source SHA-256: 53d673d78e7936880481be1017c37ca95a1e4b64419d74d32035ce703d06c7d4

static UInt64 Get_Lzma2_ChunkSize(UInt64 dict)
{
  // we use same default chunk sizes as defined in 7z encoder and lzma2 encoder
  UInt64 cs = (UInt64)dict << 2;
  const UInt32 kMinSize = (UInt32)1 << 20;
  const UInt32 kMaxSize = (UInt32)1 << 28;
  if (cs < kMinSize) cs = kMinSize;
  if (cs > kMaxSize) cs = kMaxSize;
  if (cs < dict) cs = dict;
  cs += (kMinSize - 1);
  cs &= ~(UInt64)(kMinSize - 1);
  return cs;
}

UInt64 CCompressDialog::GetMemoryUsage_Threads_Dict_DecompMem(UInt32 numThreads, UInt64 dict64, UInt64 &decompressMemory)
{
  decompressMemory = (UInt64)(Int64)-1;

  const UInt32 level = GetLevel2();
  if (level == 0 && !Get_ArcInfoEx().Is_Zstd())
  {
    decompressMemory = (1 << 20);
    return decompressMemory;
  }
  UInt64 size = 0;

  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  if (fi.Filter_() && level >= 9)
    size += (12 << 20) * 2 + (5 << 20);
  // UInt32 numThreads = GetNumThreads2();
  
  UInt32 numMainZipThreads = 1;

  if (IsZipFormat())
  {
    UInt32 numSubThreads = 1;
    if (GetMethodID() == kLZMA && numThreads > 1 && level >= 5)
      numSubThreads = 2;
    numMainZipThreads = numThreads / numSubThreads;
    if (numMainZipThreads > 1)
      size += (UInt64)numMainZipThreads * ((size_t)sizeof(size_t) << 23);
    else
      numMainZipThreads = 1;
  }
  
  const int methodId = GetMethodID();

  if (dict64 == (UInt64)(Int64)-1
      // && methodId != kZSTD
      )
    return (UInt64)(Int64)-1;

  
  switch (methodId)
  {
    case kLZMA:
    case kLZMA2:
    {
      const UInt32 dict = (dict64 >= kLzmaMaxDictSize ? kLzmaMaxDictSize : (UInt32)dict64);
      UInt32 hs = dict - 1;
      hs |= (hs >> 1);
      hs |= (hs >> 2);
      hs |= (hs >> 4);
      hs |= (hs >> 8);
      hs >>= 1;
      if (hs >= (1 << 24))
        hs >>= 1;
      hs |= (1 << 16) - 1;
      // if (numHashBytes >= 5)
      if (level < 5)
        hs |= (256 << 10) - 1;
      hs++;
      UInt64 size1 = (UInt64)hs * 4;
      size1 += (UInt64)dict * 4;
      if (level >= 5)
        size1 += (UInt64)dict * 4;
      size1 += (2 << 20);

      UInt32 numThreads1 = 1;
      if (numThreads > 1 && level >= 5)
      {
        size1 += (2 << 20) + (4 << 20);
        numThreads1 = 2;
      }
      
      UInt32 numBlockThreads = numThreads / numThreads1;
    
      UInt64 chunkSize = 0; // it's solid chunk

      if (methodId != kLZMA && numBlockThreads != 1)
      {
        chunkSize = Get_Lzma2_ChunkSize(dict);

        if (IsXzFormat())
        {
          UInt32 blockSizeLog = GetBlockSizeSpec();
          if (blockSizeLog != (UInt32)(Int32)-1)
          {
            if (blockSizeLog == kSolidLog_FullSolid)
            {
              numBlockThreads = 1;
              chunkSize = 0;
            }
            else if (blockSizeLog != kSolidLog_NoSolid)
              chunkSize = (UInt64)1 << blockSizeLog;
          }
        }
      }

      if (chunkSize == 0)
      {
        const UInt32 kBlockSizeMax = (UInt32)0 - (UInt32)(1 << 16);
        UInt64 blockSize = (UInt64)dict + (1 << 16)
          + (numThreads1 > 1 ? (1 << 20) : 0);
        blockSize += (blockSize >> (blockSize < ((UInt32)1 << 30) ? 1 : 2));
        if (blockSize >= kBlockSizeMax)
          blockSize = kBlockSizeMax;
        size += numBlockThreads * (size1 + blockSize);
      }
      else
      {
        size += numBlockThreads * (size1 + chunkSize);
        const UInt32 numPackChunks = numBlockThreads + (numBlockThreads / 8) + 1;
        if (chunkSize < ((UInt32)1 << 26)) numBlockThreads++;
        if (chunkSize < ((UInt32)1 << 24)) numBlockThreads++;
        if (chunkSize < ((UInt32)1 << 22)) numBlockThreads++;
        size += numPackChunks * chunkSize;
      }

      decompressMemory = dict + (2 << 20);
      return size;
    }

    /*
    case kZSTD:
    {
      CZstdEncProps props;
      ZstdEncProps_Init(&props);
      // props.level_zstd = level;
      props.level_7z = level;
      props.nbWorkers = numThreads;
      ZstdEncProps_SetDictProps_From_CompressDialog(&props, *this);
      ZstdEncProps_NormalizeFull(&props);
      size = ZstdEncProps_GetMemUsage(&props);
      decompressMemory = (UInt64)1 << props.windowLog;
      return size;
    }
    */
    
    case kPPMd:
    {
      decompressMemory = dict64 + (2 << 20);
      return size + decompressMemory;
    }
    
    case kDeflate:
    case kDeflate64:
    {
      UInt64 size1 = 3 << 20;
      // if (level >= 7)
        size1 += (1 << 20);
      size += size1 * numMainZipThreads;
      decompressMemory = (2 << 20);
      return size;
    }
    
    case kBZip2:
    {
      decompressMemory = (7 << 20);
      UInt64 memForOneThread = (10 << 20);
      return size + memForOneThread * numThreads;
    }
    
    case kPPMdZip:
    {
      decompressMemory = dict64 + (2 << 20);
      return size + (UInt64)decompressMemory * numThreads;
    }
  }
  
  return (UInt64)(Int64)-1;
}

UInt32 CCompressDialog::AutoThreads(UInt32 numCPUs, UInt32 numHardwareThreads)
{
  const int methodID = GetMethodID();
  const bool isZip = IsZipFormat();

  UInt32 numAlgoThreadsMax = numHardwareThreads * 2; // for unknow methods
  if (isZip)
    numAlgoThreadsMax =
        8 << (sizeof(size_t) / 2); // 32 threads for 32-bit : 128 threads for 64-bit
  else if (IsXzFormat())
    numAlgoThreadsMax = 256 * 2; // MTCODER_THREADS_MAX * 2
  else switch (methodID)
  {
    case kLZMA: numAlgoThreadsMax = 2; break;
    case kLZMA2: numAlgoThreadsMax = 256 * 2; break; // MTCODER_THREADS_MAX * 2
    case kBZip2: numAlgoThreadsMax = 64; break;
    // case kZSTD: numAlgoThreadsMax = num_ZSTD_threads_MAX; break;
    case kCopy:
    case kPPMd:
    case kDeflate:
    case kDeflate64:
    case kPPMdZip:
      numAlgoThreadsMax = 1;
  }
  UInt32 autoThreads = numCPUs;
  if (autoThreads > numAlgoThreadsMax)
      autoThreads = numAlgoThreadsMax;

  const UInt64 memUse_Limit = Get_MemUse_Bytes();

  if (_ramSize_Defined)
  if (autoThreads > 1
      // || (autoThreads == 0 && methodID == kZSTD)
      )
  {
    if (isZip)
    {
      for (; autoThreads > 1; autoThreads--)
      {
        const UInt64 dict64 = GetDict2();
        UInt64 decompressMemory;
        const UInt64 usage = GetMemoryUsage_Threads_Dict_DecompMem(autoThreads, dict64, decompressMemory);
        if (usage <= memUse_Limit)
          break;
      }
    }
    else if (methodID == kLZMA2)
    {
      const UInt64 dict64 = GetDict2();
      const UInt32 numThreads1 = (GetLevel2() >= 5 ? 2 : 1);
      UInt32 numBlockThreads = autoThreads / numThreads1;
      for (; numBlockThreads > 1; numBlockThreads--)
      {
        autoThreads = numBlockThreads * numThreads1;
        UInt64 decompressMemory;
        const UInt64 usage = GetMemoryUsage_Threads_Dict_DecompMem(autoThreads, dict64, decompressMemory);
        if (usage <= memUse_Limit)
          break;
      }
      autoThreads = numBlockThreads * numThreads1;
    }
    /*
    else if (methodID == kZSTD)
    {
      if (num_ZSTD_threads_MAX != 0)
      {
        CZstdEncProps props;
        ZstdEncProps_Init(&props);
        // props.level_zstd = level;
        props.level_7z = GetLevel2();
        ZstdEncProps_SetDictProps_From_CompressDialog(&props, *this);
        autoThreads = ZstdEncProps_GetNumThreads_for_MemUsageLimit(&props, memUse_Limit, autoThreads);
      }
    }
    */
  }

  return autoThreads;
}

UInt32 CCompressDialog::MaximumThreads(UInt32 numHardwareThreads)
{
  const int methodID = GetMethodID();
  const bool isZip = IsZipFormat();

  UInt32 numAlgoThreadsMax = numHardwareThreads * 2; // for unknow methods
  if (isZip)
    numAlgoThreadsMax =
        8 << (sizeof(size_t) / 2); // 32 threads for 32-bit : 128 threads for 64-bit
  else if (IsXzFormat())
    numAlgoThreadsMax = 256 * 2; // MTCODER_THREADS_MAX * 2
  else switch (methodID)
  {
    case kLZMA: numAlgoThreadsMax = 2; break;
    case kLZMA2: numAlgoThreadsMax = 256 * 2; break; // MTCODER_THREADS_MAX * 2
    case kBZip2: numAlgoThreadsMax = 64; break;
    // case kZSTD: numAlgoThreadsMax = num_ZSTD_threads_MAX; break;
    case kCopy:
    case kPPMd:
    case kDeflate:
    case kDeflate64:
    case kPPMdZip:
      numAlgoThreadsMax = 1;
  }
  return numAlgoThreadsMax;
}

UInt64 CCompressDialog::AutoSolid(UInt64 dict)
{
  const bool is7z = format == "7z";
  const UInt64 cs = Get_Lzma2_ChunkSize(dict);

  // Solid Block Size
  UInt64 blockSize = cs; // for xz

  if (is7z)
  {
    // we use same default block sizes as defined in 7z encoder
    UInt64 kMaxSize = (UInt64)1 << 32;
    const int methodId = GetMethodID();
    if (methodId == kLZMA2)
    {
      blockSize = cs << 6;
      kMaxSize = (UInt64)1 << 34;
    }
    else
    {
      UInt64 dict2 = dict;
      if (methodId == kBZip2)
      {
        dict2 /= 100000;
        if (dict2 < 1)
          dict2 = 1;
        dict2 *= 100000;
      }
      blockSize = dict2 << 7;
    }

    const UInt32 kMinSize = (UInt32)1 << 24;
    if (blockSize < kMinSize) blockSize = kMinSize;
    if (blockSize > kMaxSize) blockSize = kMaxSize;
  }

  return blockSize;
}
