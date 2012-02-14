-- A simple wrapper for PCRE that uses a cache for compiled expressions.

_regex_cache = {}

_regex_cache.mt = {}
setmetatable(_regex_cache, _regex_cache.mt)


_regex_cache.mt.__index = function (self, key)
    local zero
    if _VERSION == 'Lua 5.1' then zero = '%z' else zero = '\0' end
    local _, _, pattern, cflags = string.find(key, '^(.*)' .. zero .. '(.*)$')
    local _, compiled = ifre.compile(pattern, tonumber(cflags))
    self[key] = compiled
    return compiled
end


function regex_search(pattern, subject, cflags, eflags)
    _check_required(pattern, 'string')
    _check_required(subject, 'string')
    _check_optional(cflags, 'table')
    _check_optional(eflags, 'table')

    if cflags == nil then cflags = {} end
    if eflags == nil then eflags = {} end
    local flags = ifre.flags()
    cf = 0
    for _, f in ipairs(cflags) do cf = cf + flags[f] end
    ef = 0
    for _, f in pairs(eflags) do ef = cf + flags[f] end
    local compiled = _regex_cache[pattern .. '\0' .. cf]
    if compiled == nil then return nil end
    return ifre.exec(compiled, subject, ef)
end
