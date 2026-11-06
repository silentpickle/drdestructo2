#include "stdafx.h"
#include "AIProcessor.h"
#include "AIProtoManager.h"
#include "AICommands.h"
#include "ISystem.h"
#include "Logger.h"
#include "Vars.h"

namespace
{
enum TriggerKind { TIMER, DX, DY, ETERNAL, ONCE };

IScriptContext & Context(lua_State *L)
{
    return *static_cast<IScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
}

int DescribeTrigger(lua_State *L)
{
    const auto kind = lua_tointeger(L, lua_upvalueindex(1));
    const float value = kind <= DY ? luaL_checknumber(L, 1) : 0;
    lua_createtable(L, 2, 0);
    lua_pushinteger(L, kind);
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, value);
    lua_rawseti(L, -2, 2);
    return 1;
}

int Control(lua_State *L)
{
    const int control = luaL_checkinteger(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    lua_rawgeti(L, 3, 1);
    const auto kind = luaL_checkinteger(L, -1);
    lua_rawgeti(L, 3, 2);
    const float value = luaL_checknumber(L, -1);

    IAITrigger *trigger;
    switch (kind)
    {
        case TIMER: trigger = new AITrigger_Timer(value); break;
        case DX: trigger = new AITrigger_DeltaX(value); break;
        case DY: trigger = new AITrigger_DeltaY(value); break;
        case ETERNAL: trigger = new AITrigger_Eternal(); break;
        case ONCE: trigger = new AITrigger_OneTime(); break;
        default: return luaL_error(L, "Unknown AI trigger");
    }
    Context(L).Control(control, trigger);
    return lua_yield(L, 0);
}

void SetNumber(lua_State *L, const char *field, lua_Number value)
{
    lua_pushnumber(L, value);
    lua_setfield(L, -2, field);
}

int CreateParams(lua_State *L)
{
    const auto data = Context(L).CreateObjectParams();
    lua_createtable(L, 0, 8);
    SetNumber(L, "x", data.x);
    SetNumber(L, "y", data.y);
    SetNumber(L, "dir", data.dir);
    SetNumber(L, "angle", data.angle);
    SetNumber(L, "speedX", data.speedX);
    SetNumber(L, "speedY", data.speedY);
    lua_pushboolean(L, data.hasSpeed);
    lua_setfield(L, -2, "hasSpeed");
    lua_pushlstring(L, data.ai.data(), data.ai.size());
    lua_setfield(L, -2, "ai");
    return 1;
}

double NumberField(lua_State *L, const char *field)
{
    lua_getfield(L, 3, field);
    const auto value = luaL_checknumber(L, -1);
    lua_pop(L, 1);
    return value;
}

int CreateObject(lua_State *L)
{
    const char *prototype = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    const bool replace = lua_toboolean(L, 4);
    const int x = NumberField(L, "x");
    const int y = NumberField(L, "y");
    const auto direction = static_cast<EDirection>(static_cast<int>(NumberField(L, "dir")));
    const float angle = NumberField(L, "angle");
    const float speedX = NumberField(L, "speedX");
    const float speedY = NumberField(L, "speedY");
    lua_getfield(L, 3, "hasSpeed");
    const bool hasSpeed = lua_toboolean(L, -1);
    lua_getfield(L, 3, "ai");
    const char *ai = luaL_checkstring(L, -1);

    // Destroy C++ temporaries before Lua yields through the C callback.
    {
        SCreateObjectRequest::SData data;
        data.x = x;
        data.y = y;
        data.dir = direction;
        data.angle = angle;
        data.speedX = speedX;
        data.speedY = speedY;
        data.hasSpeed = hasSpeed;
        data.ai = ai;
        Context(L).CreateObject(prototype, data, replace);
    }
    return lua_yield(L, 0);
}

int DestroySelf(lua_State *L)
{
    Context(L).DestroySelf();
    return lua_yield(L, 0);
}

int SelfX(lua_State *L)
{
    lua_pushnumber(L, Context(L).GetSelfX());
    return 1;
}

int SelfY(lua_State *L)
{
    lua_pushnumber(L, Context(L).GetSelfY());
    return 1;
}

int RandomInt(lua_State *L)
{
    lua_pushinteger(L, RndInt(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2)));
    return 1;
}

int RandomFloat(lua_State *L)
{
    lua_pushnumber(L, RndFloat(luaL_checknumber(L, 1), luaL_checknumber(L, 2)));
    return 1;
}
}

AILuaState::AILuaState(lua_State *state, int key) : m_pState(state), m_key(key) {}

AILuaState::~AILuaState()
{
    if (m_key == LUA_NOREF)
        lua_close(m_pState);
    else
        luaL_unref(m_pState, LUA_REGISTRYINDEX, m_key);
}

void AILuaState::Run(const char *func, IScriptContext &context)
{
    int arguments = 0;
    if (lua_status(m_pState) != LUA_YIELD)
    {
        lua_getglobal(m_pState, func);
        lua_newtable(m_pState);
        const luaL_Reg methods[] = {
            {"Control", Control}, {"CreateObjectParams", CreateParams},
            {"CreateObject", CreateObject}, {"DestroySelf", DestroySelf},
            {"GetSelfX", SelfX}, {"GetSelfY", SelfY}, {nullptr, nullptr}
        };
        lua_pushlightuserdata(m_pState, &context);
        luaL_setfuncs(m_pState, methods, 1);
        arguments = 1;
    }
    int results = 0;
    const int status = lua_resume(m_pState, nullptr, arguments, &results);
    if (status == LUA_YIELD)
    {
        lua_pop(m_pState, results);
        return;
    }
    if (status != LUA_OK)
    {
        const char *error = lua_tostring(m_pState, -1);
        GetLog().Log(0, LL_CRITICAL, "AI script %s: %s", func, error ? error : "unknown error");
        lua_resetthread(m_pState);
        context.Control(0, new AITrigger_Eternal());
    }
    lua_settop(m_pState, 0);
}

AIProtoManager::AIProtoManager() : m_state(luaL_newstate(), LUA_NOREF)
{
    luaL_openlibs(m_state.GetLuaState());
}

AIProtoManager::~AIProtoManager() = default;

IAIState *AIProtoManager::CreateAIState()
{
    lua_State *state = m_state.GetLuaState();
    lua_State *thread = lua_newthread(state);
    return new AILuaState(thread, luaL_ref(state, LUA_REGISTRYINDEX));
}

bool AIProtoManager::LoadResources()
{
    lua_State *state = m_state.GetLuaState();
    if (luaL_dofile(state, "Data/Protos/ai.lua") == LUA_OK)
        return true;
    GetLog().Log(0, LL_CRITICAL, "Loading AI scripts: %s", lua_tostring(state, -1));
    lua_pop(state, 1);
    return false;
}

AIProtoManager & GetAI()
{
    static AIProtoManager manager;
    return manager;
}

void LoadAI()
{
    GetAI().LoadResources();
}

void AIProcessor::RegisterFunctions(IAIState *state)
{
    lua_State *L = static_cast<AILuaState *>(state)->GetLuaState();
    lua_register(L, "RandomInt", RandomInt);
    lua_register(L, "RandomFloat", RandomFloat);
    const char *const triggers[] = {
        "Trigger_Timer", "Trigger_DX", "Trigger_DY", "Trigger_Eternal", "Trigger_OneTime"
    };
    for (int kind = TIMER; kind <= ONCE; ++kind)
    {
        lua_pushinteger(L, kind);
        lua_pushcclosure(L, DescribeTrigger, 1);
        lua_setglobal(L, triggers[kind]);
    }

    lua_newtable(L);
    SetNumber(L, "IDLE", 0);
    const char *const controls[] = {
        "VERT_UP", "VERT_DOWN", "SPEED_UP", "STOP", "GO", "FLIP", "FIRE1", "FIRE2", "RANDOM_TELEPORT"
    };
    for (const char *control : controls)
    {
        const std::string var = std::string("CONTROL_") + control;
        const std::string field = std::string("CTRL_") + control;
        SetNumber(L, field.c_str(), GetVars().GetVar<int>(var)->m_value);
    }
    lua_setglobal(L, "Context");
}
