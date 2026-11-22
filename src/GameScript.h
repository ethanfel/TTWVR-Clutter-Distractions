// SPDX-License-Identifier: GPL-3.0-only
#pragma once
struct Value { union { double number{}; unsigned char* object; }; unsigned char type{}; };
static_assert(sizeof(Value)==16);
struct Scripts { bool (__cdecl* call)(void*,void*,void*,Value*,unsigned char,...);
    void *params,*extract,*format,*callAlt; void* (__cdecl* compile)(const char*); };
struct Arrays { void* before[6]; unsigned (__cdecl* size)(void*); void* lookup;
    bool (__cdecl* get)(void*,const Value&,Value&);
    bool (__cdecl* elements)(void*,Value*,Value*); };
