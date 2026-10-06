// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "CPP/7zip/UI/Common/Bench.h"
#include <string>
namespace PortBenchmark {
bool Requested();
HRESULT Run(DECL_EXTERNAL_CODECS_LOC_VARS const CObjectVector<CProperty> &props, UInt32 passes);
// Same presentation used for Current, Resulting and accumulated Total.
std::string ResultValue(const CTotalBenchRes &result, UInt64 unpackSize);
}
