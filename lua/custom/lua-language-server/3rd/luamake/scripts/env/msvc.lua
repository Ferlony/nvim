local find_msvc = require "env.find_msvc"
local fs = require "bee.filesystem"
local fsutil = require "fsutil"
local globals = require "globals"

local m = {}
local env
local prefix

local ArchAlias = {
    x86_64 = "x64",
    x86 = "x86",
}

local function getConsoleCP()
    local f = io.popen("chcp", "r")
    if f then
        local data = f:read "a"
        return data:match "%d+"
    end
end

local function readEnvConfig()
    local EnvConfig = fsutil.join(WORKDIR, globals.builddir, "env.lua")
    local f <close> = assert(io.open(EnvConfig, "r"))
    local data = assert(f:read "a")
    local config = assert(load(data, "t", nil))()
    f:close()
    return config
end

local function writeEnvConfig(data)
    local EnvConfig = fsutil.join(WORKDIR, globals.builddir, "env.lua")
    local f <close> = assert(io.open(EnvConfig, "w"))
    f:write(data)
end

local function updateEnvConfig()
    if not env then
        local config = readEnvConfig()
        env = config.env
        prefix = config.prefix
    end
end

function m.hasEnvConfig()
    local EnvConfig = fsutil.join(WORKDIR, globals.builddir, "env.lua")
    return fs.exists(EnvConfig)
end

function m.cleanEnvConfig()
    local EnvConfig = fsutil.join(WORKDIR, globals.builddir, "env.lua")
    fs.remove(EnvConfig)
end

function m.createEnvConfig(arch, rebuild)
    local console_cp = getConsoleCP()
    if not rebuild and m.hasEnvConfig() then
        local config = readEnvConfig()
        if config.arch == arch
            and (not console_cp or config.console_cp == console_cp)
            and config.toolspath and fs.exists(config.toolspath)
        then
            env = config.env
            prefix = config.prefix
            return
        end
    end
    local winsdk = find_msvc.findwinsdk()
    env = find_msvc.environment(winsdk, ArchAlias[arch])
    prefix = find_msvc.prefix(env)

    local s = {}
    s[#s+1] = "return {"
    s[#s+1] = ("arch=%q,"):format(arch)
    s[#s+1] = ("toolspath=%q,"):format(find_msvc.toolspath())
    s[#s+1] = ("console_cp=%q,"):format(console_cp)
    if winsdk then
        s[#s+1] = ("winsdk=%q,"):format(winsdk)
    end
    s[#s+1] = ("prefix=%q,"):format(prefix)
    s[#s+1] = "env={"
    for name, value in pairs(env) do
        s[#s+1] = ("%s=%q,"):format(name, value)
    end
    s[#s+1] = "},"
    s[#s+1] = "}"
    s[#s+1] = ""
    writeEnvConfig(table.concat(s, "\n"))
end

function m.getEnv()
    updateEnvConfig()
    return env
end

function m.getPrefix()
    updateEnvConfig()
    return prefix
end

function m.archAlias(arch)
    return ArchAlias[arch]
end

m.binpath = find_msvc.binpath
m.vcrtpath = find_msvc.vcrtpath
m.ucrtpath = find_msvc.ucrtpath

return m
