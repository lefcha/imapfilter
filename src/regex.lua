-- A simple wrapper for PCRE that uses a cache for compiled expressions.

_regex_cache = {}

_regex_cache.mt = {}
setmetatable(_regex_cache, _regex_cache.mt)


_regex_cache.mt.__index = function (self, key)
    local r, compiled = ifre.compile(key)
    if not r then return end
    self[key] = compiled
    return compiled
end


function regex_search(pattern, subject)
    _check_required(pattern, 'string')
    _check_required(subject, 'string')

    local compiled = _regex_cache[pattern]
    if compiled == nil then return nil end
    return ifre.exec(compiled, subject)
end
