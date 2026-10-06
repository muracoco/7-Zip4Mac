// Modified for 7-Zip Mac Port by the 7-Zip4Mac contributors.
// Port adaptation: 2026-10-05 to 2026-10-06 (recorded Git history).
// Modification notice added on 2026-10-06; original notices retained below.
// Common/Wildcard.cpp
// Copyright (C) 1999-2026 Igor Pavlov. GNU LGPL-2.1-or-later.
// Original EnhancedMaskTest; SHA-256: 8c009138344c21eb8aea64334e892cb4949417e7bbc30cd75941692cd595f122
static bool EnhancedMaskTest(const wchar_t *mask, const wchar_t *name)
{
  for (;;)
  {
    const wchar_t m = *mask;
    const wchar_t c = *name;
    if (m == 0)
      return (c == 0);
    if (m == '*')
    {
      if (EnhancedMaskTest(mask + 1, name))
        return true;
      if (c == 0)
        return false;
    }
    else
    {
      if (m == '?')
      {
        if (c == 0)
          return false;
      }
      else if (m != c)
        if (g_CaseSensitive || MyCharUpper(m) != MyCharUpper(c))
          return false;
      mask++;
    }
    name++;
  }
}
