// Modified for 7-Zip Mac Port by the 7-Zip4Mac contributors.
// Port adaptation: 2026-10-04 to 2026-10-05 (recorded Git history).
// Modification notice added on 2026-10-06; original notices retained below.
// CompressDialog.cpp
// Copyright (C) 1999-2026 Igor Pavlov. GNU LGPL-2.1-or-later.
// Generated from official 7-Zip 26.03 by scripts/import-compression-math.py.
// Original arithmetic bodies retained. Thread/solid bodies omit Win32 combo
// plumbing; signatures return results through the adapter in CompressionMath.cpp.
// Original source SHA-256: 53d673d78e7936880481be1017c37ca95a1e4b64419d74d32035ce703d06c7d4

// Numeric combo bodies omit registry lookup and Win32 geometry; saved values are arguments.

void CCompressDialog::SetDictionary2(UInt32 defaultDict)
{
  m_Dictionary.ResetContent();
  _auto_Dict = (UInt32)(Int32)-1;
  const int methodID = GetMethodID();
  const UInt32 level = GetLevel2();
  if (methodID < 0)
    return;
  
  switch (methodID)
  {
    case kLZMA:
    case kLZMA2:
    {
      {
        _auto_Dict = level <= 4 ?
            (UInt32)1 << (level * 2 + 16) :
            level <= sizeof(size_t) / 2 + 4 ?
              (UInt32)1 << (level + 20) :
              (UInt32)1 << (sizeof(size_t) / 2 + 24);
      }

      // we use threshold 3.75 GiB to switch to kLzmaMaxDictSize.
      if (defaultDict != (UInt32)(Int32)-1
          && defaultDict >= ((UInt32)15 << 28))
        defaultDict = kLzmaMaxDictSize;
      
      const size_t kLzmaMaxDictSize_Up = (size_t)1 << (20 + sizeof(size_t) / 4 * 6);
      
      int curSel = AddDict2(k_Auto_Dict, _auto_Dict);

      for (unsigned i = (16 - 1) * 2; i <= (32 - 1) * 2; i++)
      {
        if (i < (20 - 1) * 2
            && i != (16 - 1) * 2
            && i != (18 - 1) * 2)
          continue;
        if (i == (20 - 1) * 2 + 1)
          continue;
        const size_t dict_up = (size_t)(2 + (i & 1)) << (i / 2);
        size_t dict = dict_up;
        if (dict_up >= kLzmaMaxDictSize)
          dict = kLzmaMaxDictSize; // we reduce dictionary
        
        const int index = AddDict(dict);
        // AddDict2(dict, dict_up); // for debug : we show 4 GB

        // const UInt32 numThreads = 2;
        // const UInt64 memUsage = GetMemoryUsageComp_Threads_Dict(numThreads, dict);
        if (defaultDict != (UInt32)(Int32)-1)
          if (dict <= defaultDict || curSel <= 0)
          // if (!maxRamSize_Defined || memUsage <= maxRamSize)
            curSel = index;
        if (dict_up >= kLzmaMaxDictSize_Up)
          break;
      }
      
      m_Dictionary.SetCurSel(curSel);
      break;
    }

    /*
    case kZSTD:
    {
      if (defaultDict != (UInt32)(Int32)-1 &&
          defaultDict > kZstd_MAX_DictSize)
        defaultDict = kZstd_MAX_DictSize;

      if (defaultDict_Chain != (UInt32)(Int32)-1 &&
          defaultDict_Chain > kZstd_MAX_DictSize_Chain)
        defaultDict_Chain = kZstd_MAX_DictSize_Chain;

      {
        CZstdEncProps props;
        ZstdEncProps_Init(&props);
        // props.level_zstd = level;
        props.level_7z = level;
        ZstdEncProps_Set_WindowSize(&props, defaultDict != (UInt32)(Int32)-1 ? defaultDict: 0);
        ZstdEncProps_NormalizeFull(&props);
        _auto_Dict_Chain = (UInt32)1 << props.windowLog_Chain;
      }
      {
        CZstdEncProps props;
        ZstdEncProps_Init(&props);
        // props.level_zstd = level;
        props.level_7z = level;
        ZstdEncProps_Set_WindowChainSize(&props, defaultDict_Chain != (UInt32)(Int32)-1 ? defaultDict_Chain: 0);
        ZstdEncProps_NormalizeFull(&props);
        _auto_Dict = (UInt32)1 << props.windowLog;
      }

      // if there is collision of two window sizes, we reduce dict_Chain
      if (defaultDict != (UInt32)(Int32)-1 &&
          defaultDict_Chain != (UInt32)(Int32)-1 &&
          defaultDict < defaultDict_Chain)
        defaultDict_Chain = defaultDict;

      {
        int curSel = AddDict2(k_Auto_Dict, _auto_Dict);

        // defaultDict = 12 << 10; // for debug
        const UInt32 kWinStart = 18;
        if (defaultDict != 0 && defaultDict < ((UInt32)1 << kWinStart))
          curSel = AddDict(defaultDict);
        
        for (unsigned i = kWinStart; i <= MY_ZSTD_WINDOWLOG_MAX; i++)
        {
          const size_t dict = (size_t)1 << i;
          const int index = AddDict(dict);
          if (defaultDict != (UInt32)(Int32)-1)
            if (dict <= defaultDict || curSel <= 0)
              curSel = index;
        }
        m_Dictionary.SetCurSel(curSel);
      }

      {
        int curSel = Combo_AddDict2(m_Dictionary_Chain, k_Auto_Dict, _auto_Dict_Chain);
        
        // defaultDict_Chain = 10 << 10; // for debug
        const UInt32 kWinChainStart = 15;
        if (defaultDict_Chain != 0 && defaultDict_Chain < ((UInt32)1 << kWinChainStart))
          curSel = AddDict_Chain(defaultDict_Chain);

        for (unsigned i = kWinChainStart; i <= kMaxDictChain; i++)
        {
          const size_t dict = (size_t)1 << i;
          if (defaultDict != (UInt32)(Int32)-1 && dict > defaultDict)
            break;
          const int index = AddDict_Chain(dict);
          if (defaultDict_Chain != (UInt32)(Int32)-1)
            if (dict <= defaultDict_Chain || curSel <= 0)
              curSel = index;
        }
        m_Dictionary_Chain.SetCurSel(curSel);
      }

      break;
    }
    */
    
    case kPPMd:
    {
      _auto_Dict = (UInt32)1 << (level + 19);

      const UInt32 kPpmd_Default_4g = (UInt32)0 - ((UInt32)1 << 10);
      const size_t kPpmd_MaxDictSize_Up = (size_t)1 << (29 + sizeof(size_t) / 8);

      if (defaultDict != (UInt32)(Int32)-1
          && defaultDict >= ((UInt32)15 << 28)) // threshold
        defaultDict = kPpmd_Default_4g;

      int curSel = AddDict2(k_Auto_Dict, _auto_Dict);

      for (unsigned i = (20 - 1) * 2; i <= (32 - 1) * 2; i++)
      {
        if (i == (20 - 1) * 2 + 1)
          continue;

        const size_t dict_up = (size_t)(2 + (i & 1)) << (i / 2);
        size_t dict = dict_up;
        if (dict_up >= kPpmd_Default_4g)
          dict = kPpmd_Default_4g;

        const int index = AddDict2(dict, dict_up);
        // AddDict2((UInt32)((UInt32)0 - 2), dict_up); // for debug
        // AddDict(dict_up); // for debug
        // const UInt64 memUsage = GetMemoryUsageComp_Threads_Dict(1, dict);
        if (defaultDict != (UInt32)(Int32)-1)
          if (dict <= defaultDict || curSel <= 0)
            // if (!maxRamSize_Defined || memUsage <= maxRamSize)
            curSel = index;
        if (dict_up >= kPpmd_MaxDictSize_Up)
          break;
      }
      m_Dictionary.SetCurSel(curSel);
      break;
    }

    case kPPMdZip:
    {
      _auto_Dict = (UInt32)1 << (level + 19);
      
      int curSel = AddDict2(k_Auto_Dict, _auto_Dict);

      for (unsigned i = 20; i <= 28; i++)
      {
        const UInt32 dict = (UInt32)1 << i;
        const int index = AddDict(dict);
        // const UInt64 memUsage = GetMemoryUsageComp_Threads_Dict(1, dict);
        if (defaultDict != (UInt32)(Int32)-1)
          if (dict <= defaultDict || curSel <= 0)
            // if (!maxRamSize_Defined || memUsage <= maxRamSize)
            curSel = index;
      }
      m_Dictionary.SetCurSel(curSel);
      break;
    }

    case kDeflate:
    case kDeflate64:
    {
      const UInt32 dict = (methodID == kDeflate ? (UInt32)(1 << 15) : (UInt32)(1 << 16));
      _auto_Dict = dict;
      AddDict2(k_Auto_Dict, _auto_Dict);
      m_Dictionary.SetCurSel(0);
      // EnableItem(IDC_COMPRESS_DICTIONARY, false);
      break;
    }
    
    case kBZip2:
    {
      {
             if (level >= 5) _auto_Dict = (900 << 10);
        else if (level >= 3) _auto_Dict = (500 << 10);
        else                 _auto_Dict = (100 << 10);
      }

      int curSel = AddDict2(k_Auto_Dict, _auto_Dict);
      
      for (unsigned i = 1; i <= 9; i++)
      {
        const UInt32 dict = ((UInt32)i * 100) << 10;
        AddDict(dict);
        // AddDict2(i * 100000, dict);
        if (defaultDict != (UInt32)(Int32)-1)
          if (i <= defaultDict / 100000 || curSel <= 0)
            curSel = m_Dictionary.GetCount() - 1;
      }
      m_Dictionary.SetCurSel(curSel);
      break;
    }

    case kCopy:
    {
      _auto_Dict = 0;
      AddDict(0);
      m_Dictionary.SetCurSel(0);
      break;
    }
  }
}

void CCompressDialog::SetOrder2(UInt32 defaultOrder)
{
  m_Order.ResetContent();
  _auto_Order = 1;
  const int methodID = GetMethodID();
  const UInt32 level = GetLevel2();
  if (methodID < 0)
    return;
  
  switch (methodID)
  {
    case kLZMA:
    case kLZMA2:
    {
      _auto_Order = (level < 7 ? 32 : 64);
      int curSel = AddOrder_Auto();
      for (unsigned i = 2 * 2; i < 8 * 2; i++)
      {
        UInt32 order = ((UInt32)(2 + (i & 1)) << (i / 2));
        if (order > 256)
          order = 273;
        const int index = AddOrder(order);
        if (defaultOrder != (UInt32)(Int32)-1)
          if (order <= defaultOrder || curSel <= 0)
            curSel = index;
      }
      m_Order.SetCurSel(curSel);
      break;
    }

    /*
    case kZSTD:
    {
      {
        CZstdEncProps props;
        ZstdEncProps_Init(&props);
        // props.level_zstd = level;
        props.level_7z = level;
        ZstdEncProps_NormalizeFull(&props);
        _auto_Order = props.targetLength;
        if (props.strategy < ZSTD_strategy_btopt)
        {
          // ZSTD_strategy_fast uses targetLength to change fast level.
          // targetLength probably is used only in ZSTD_strategy_btopt and higher
          break;
        }
      }
      int curSel = AddOrder_Auto();

      for (unsigned i = 6; i <= 9 * 2; i++)
      {
        UInt32 order = ((UInt32)(2 + (i & 1)) << (i / 2));
        // if (order > 999) order = 999;
        const int index = AddOrder(order);
        if (defaultOrder != (UInt32)(Int32)-1)
          if (order <= defaultOrder || curSel <= 0)
            curSel = index;
      }
      m_Order.SetCurSel(curSel);
      break;
    }
    */

    case kDeflate:
    case kDeflate64:
    {
      {
             if (level >= 9) _auto_Order = 128;
        else if (level >= 7) _auto_Order = 64;
        else                 _auto_Order = 32;
      }
      int curSel = AddOrder_Auto();
      for (unsigned i = 2 * 2; i < 8 * 2; i++)
      {
        UInt32 order = ((UInt32)(2 + (i & 1)) << (i / 2));
        if (order > 256)
          order = (methodID == kDeflate64 ? 257 : 258);
        const int index = AddOrder(order);
        if (defaultOrder != (UInt32)(Int32)-1)
          if (order <= defaultOrder || curSel <= 0)
            curSel = index;
      }
      
      m_Order.SetCurSel(curSel);
      break;
    }
   
    case kPPMd:
    {
      {
             if (level >= 9) _auto_Order = 32;
        else if (level >= 7) _auto_Order = 16;
        else if (level >= 5) _auto_Order = 6;
        else                 _auto_Order = 4;
      }
      
      int curSel = AddOrder_Auto();

      for (unsigned i = 0;; i++)
      {
        UInt32 order = i + 2;
        if (i >= 2)
          order = (4 + ((i - 2) & 3)) << ((i - 2) / 4);
        const int index = AddOrder(order);
        if (defaultOrder != (UInt32)(Int32)-1)
          if (order <= defaultOrder || curSel <= 0)
            curSel = index;
        if (order >= 32)
          break;
      }
      m_Order.SetCurSel(curSel);
      break;
    }

    case kPPMdZip:
    {
      _auto_Order = level + 3;
      int curSel = AddOrder_Auto();
      for (unsigned i = 2; i <= 16; i++)
      {
        const int index = AddOrder(i);
        if (defaultOrder != (UInt32)(Int32)-1)
          if (i <= defaultOrder || curSel <= 0)
            curSel = index;
      }
      m_Order.SetCurSel(curSel);
      break;
    }
    
    // case kBZip2:
    default:
      break;
  }
}
