/*
** $Id: lapi.h,v 2.2.1.1 2007/12/27 13:02:25 roberto Exp $
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/

#ifndef lapi_h
#define lapi_h


#include "lobject.h"
#include "lstate.h"


LUAI_FUNC void luaA_pushobject (lua_State *L, const TValue *o);

/* This was moved out of lapi.c into the header to let it inline. MSVC's
 * __inline merges the copies; a plain definition in a header emits one in
 * every translation unit that includes it, which the GNU linker rejects. For
 * anything but MSVC the definition stays in lapi.c, as upstream Lua has it. */
#ifdef _MSC_VER
_inline LUA_API int lua_gettop (lua_State *L) {
  return cast_int(L->top - L->base);
}
#else
LUA_API int lua_gettop (lua_State *L);
#endif

#endif
