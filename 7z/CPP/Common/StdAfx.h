// StdAfx.h

// Every other directory in this tree has one of these; CPP/Common did not,
// because MSVC never opens it: with PrecompiledHeader set to Use, the
// #include "StdAfx.h" line is only a marker for where the precompiled header
// ends. GCC has no such mechanism, so the file has to exist. Its contents
// match the sibling headers, adjusted for depth.

#ifndef __STDAFX_H
#define __STDAFX_H

#include "MyWindows.h"
#include "NewHandler.h"

#endif
