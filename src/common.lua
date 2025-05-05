-- Common functions for all classes and compatibility workarounds

if _VERSION == 'Lua 5.1' then
    table.unpack = unpack
else
    unpack = table.unpack
end


function _check_required(arg, argtype)
    if type(arg) == 'nil' then
        error('required argument left out', 3)
    else
        _check_optional(arg, argtype)
    end
end

function _check_optional(arg, argtype)
    if type(arg) ~= 'nil' then
        if type(argtype) == 'string' then
            if type(arg) ~= argtype then
                error(argtype .. ' argument expected, got ' .. type(arg), 4)
            end
        elseif type(argtype) == 'table' then
            local b = false
            for _, t in ipairs(argtype) do
                if type(arg) == t then b = true end
            end
            if b == false then
                error(argtype .. ' argument expected, got ' .. type(arg), 4)
            end
        end
    end
end


function _extract_mailboxes(messages)
    local t = {}
    for _, v in ipairs(messages) do
        b, _ = table.unpack(v)
        t[b] = true
    end
    return t
end

function _extract_messages(mailbox, messages)
    local t = {}
    for _, v in ipairs(messages) do
        b, m = table.unpack(v)
        if mailbox == b then table.insert(t, m) end
    end
    return t
end


function _make_range(messages)
    for _, m in ipairs(messages) do
        if type(m) ~= 'number' then return messages end
    end

    table.sort(messages)

    local t = {}
    local a, z
    for _, m in ipairs(messages) do
        if a == nil or z == nil then
            a = m
            z = m
        else
            if m == z + 1 and m - a <= options.range then
                z = m
            else
                if a == z then
                    table.insert(t, tostring(a))
                else
                    table.insert(t, a .. ':' .. z)
                end
                a = m
                z = m
            end
        end
    end

    if a == z then
        table.insert(t, tostring(a))
    else
        table.insert(t, a .. ':' .. z)
    end

    return t
end


function _make_query(criteria, messages)
    local s = messages .. ' '

    if criteria.invert ~= true then
        for ka, va in ipairs(criteria) do
            if type(va) == 'string' then
                s = s .. '' .. '(' .. va .. ')' .. ' '
            elseif type(va) == 'table' then
                for i = 1, #va - 1 do s = s .. 'OR ' end
                for ko, vo in ipairs(va) do
                    if type(vo) ~= 'string' then
                        error('filter rule not a string', 2)
                    end
                    s = s .. '(' .. vo .. ') '
                end
            else
                error('filter element not a string or table', 2)
            end
        end
    else
        for i = 1, #criteria - 1 do s = s .. 'OR ' end
        for ko, vo in ipairs(criteria) do
            if type(vo) == 'string' then
                s = s .. '' .. '(' .. vo .. ')' .. ' '
            elseif type(vo) == 'table' then
                s = s .. '('
                for ka, va in ipairs(vo) do
                    if type(va) ~= 'string' then
                        error('filter rule not a string', 2)
                    end
                    s = s .. va .. ' '
                end
                s = string.gsub(s, '(.+) ', '%1')
                s = s .. ') '
            else
                error('filter rule not a string or table', 2)
            end
        end
    end

    s = string.gsub(s, '(.+) ', '%1')

    return s
end


function _parse_structure(b)
    local bs = _parse_body(b)
    if not bs then error(b.i .. ':' .. b.s) end
    return _parse_normalize(bs)
end

function _parse_normalize(bs, key, val)
    if not key or not val then
        if #bs == 0 then
            return { ['1'] = bs }
        else
            for k, v in pairs(bs) do
                if type(k) ~= 'number' then bs[k] = nil end
            end
            for k, v in ipairs(bs) do
                _parse_normalize(bs, k, v)
                bs[tostring(k)] = v
                bs[k] = nil
            end
        end
        return bs
    else
        for k, v in ipairs(val) do
            local new = tostring(key) .. '.' .. tostring(k)
            bs[new] = v
            _parse_normalize(bs, new, v)
            val[k] = nil
        end
    end
end

function _parse_body(b)
    if not _parse_lpar(b) then return end

    local bp
    if _parse_lpar(b, true) then
        bp = _parse_mpart(b)
    else
        bp = _parse_1part(b)
    end

    if not _parse_rpar(b) then return end

    return bp
end

function _parse_1part(b)
    local i = b.i
    local t = _parse_string(b)
    _parse_space(b)
    local s = _parse_string(b)
    if t and t:lower() == 'message' and s and s:lower() == 'rfc822' then
        return _parse_message(b)
    else
        b.i = i
        return _parse_basic(b)
    end
end

function _parse_basic(b)
    local bp = {}
    local s

    s = _parse_string(b)
    if not s then return end
    bp['type'] = s

    _parse_space(b)

    s = _parse_string(b)
    if not s then return end
    bp['type'] = bp['type'] .. '/' .. s

    _parse_space(b)

    s = _parse_param(b, 'name')
    if s then bp['name'] = s end

    _parse_space(b)
    _parse_nstring(b)
    _parse_space(b)
    _parse_nstring(b)
    _parse_space(b)
    local e = _parse_string(b)
    if e then
	    bp['encoding'] = e:lower()
    end
    _parse_space(b)
    bp['size'] = _parse_number(b)
    if bp['type']:sub(1, 5):lower() == 'text/' then
        _parse_space(b)
        _parse_number(b)
    end
    if _parse_space(b) then _parse_nstring(b) end
    if _parse_space(b) then
        s = _parse_dsp(b)
        if s then bp['name'] = s end
    end
    if _parse_space(b) then _parse_lang(b) end
    if _parse_space(b) then _parse_nstring(b) end
    while _parse_space(b) and b.i <= #b.s do _parse_extension(b) end

    return bp
end

function _parse_mpart(b)
    local bp = {}
    local i = 1
    local s

    bp['type'] = 'multipart'

    while _parse_lpar(b, true) and b.i <= #b.s do
        bp[i] = _parse_body(b)
        i = i + 1
    end

    _parse_space(b)

    s = _parse_string(b)
    if not s then return end
    bp['type'] = bp['type'] .. '/' .. s

    if _parse_space(b) then
        s = _parse_param(b, 'name')
        if s then bp['name'] = s end
    end
    if _parse_space(b) then
        s = _parse_dsp(b)
        if s then bp['name'] = s end
    end
    if _parse_space(b) then _parse_lang(b) end
    if _parse_space(b) then _parse_nstring(b) end
    while _parse_space(b) and b.i < #b.s do _parse_extension(b) end

    return bp
end

function _parse_message(b)
    local bp = {}
    local s

    bp['type'] = 'message/rfc822'

    _parse_space(b)

    s = _parse_param(b, 'name')
    if s then bp['name'] = s end

    _parse_space(b)
    _parse_nstring(b)
    _parse_space(b)
    _parse_nstring(b)
    _parse_space(b)
    _parse_string(b)
    _parse_space(b)
    bp['size'] = _parse_number(b)

    _parse_space(b)
    _parse_envelope(b)

    _parse_space(b)
    local p = _parse_body(b)
    if not p then return end
    if #p == 0 then
        bp[1] = p
    else
        for k, v in pairs(p) do
            if type(k) == 'number' then
                bp[k] = v
            end
        end
    end

    _parse_space(b)
    _parse_number(b)

    if _parse_space(b) then _parse_nstring(b) end
    if _parse_space(b) then
        s = _parse_dsp(b)
        if s then bp['name'] = s end
    end
    if _parse_space(b) then _parse_lang(b) end
    if _parse_space(b) then _parse_nstring(b) end
    while _parse_space(b) and b.i <= #b.s do _parse_extension(b) end

    return bp
end

function _parse_envelope(b)
    _parse_lpar(b)
    _parse_nstring(b)
    _parse_space(b)
    _parse_nstring(b)
    _parse_space(b)
    _parse_address(b)
    _parse_space(b)
    _parse_address(b)
    _parse_space(b)
    _parse_address(b)
    _parse_space(b)
    _parse_address(b)
    _parse_space(b)
    _parse_address(b)
    _parse_space(b)
    _parse_address(b)
    _parse_space(b)
    _parse_nstring(b)
    _parse_space(b)
    _parse_nstring(b)
    _parse_rpar(b)
end

function _parse_address(b)
    if _parse_lpar(b) then
        while _parse_lpar(b) and b.i <= #b.s do
            _parse_nstring(b)
            _parse_space(b)
            _parse_nstring(b)
            _parse_space(b)
            _parse_nstring(b)
            _parse_space(b)
            _parse_nstring(b)
            _parse_rpar(b)
        end
        _parse_rpar(b)
    elseif _parse_nil(b) then
    end
end

function _parse_lang(b)
    if _parse_lpar(b) then
        local lang = {}
        repeat
            table.insert(lang, _parse_string(b))
        until not _parse_space(b) or b.i > #b.s
        _parse_rpar(b)
        return lang
    else
        return _parse_nstring(b)
    end
end

function _parse_dsp(b)
    local r
    if _parse_lpar(b) then
        _parse_string(b)
        _parse_space(b)
        r = _parse_param(b, 'filename')
        _parse_rpar(b)
    elseif _parse_nil(b) then
    end
    return r
end

function _parse_param(b, key)
    local r
    if _parse_lpar(b) then
        repeat
            local s = _parse_string(b)
            _parse_space(b)
            if s and s:lower() == key then
                r = _parse_string(b)
            else
                _parse_string(b)
            end
        until not _parse_space(b) or b.i > #b.s
        _parse_rpar(b)
    elseif _parse_nil(b) then
    end
    return r
end

function _parse_extension(b)
    if _parse_nstring(b) then
    elseif _parse_number(b) then
    elseif _parse_lpar(b) then
        _parse_extension(b)
        while _parse_space(b) and b.i <= #b.s do
            _parse_extension(b)
        end
        _parse_rpar(b)
    else
    end
end

function _parse_space(b, peek)
    if b.s:sub(b.i, b.i) == ' ' then
        if not peek then b.i = b.i + 1 end
        return true
    else
        return false
    end
end

function _parse_lpar(b, peek)
    if b.s:sub(b.i, b.i) == '(' then
        if not peek then b.i = b.i + 1 end
        return true
    else
        return false
    end
end

function _parse_rpar(b, peek)
    if b.s:sub(b.i, b.i) == ')' then
        if not peek then b.i = b.i + 1 end
        return true
    else
        return false
    end
end

function _parse_nil(b)
    if b.s:sub(b.i, b.i + 2):upper() == 'NIL' then
        b.i = b.i + 3
        return true
    else
        return false
    end
end

function _parse_string(b)
    local i = b.i

    if b.s:sub(i, i) == '"' then
        i = i + 1
    else return end

    local j = i
    local n = 0
    while true do
        n = b.s:find('"', i + n)
        if not n then return end
        if b.s:sub(n - 1, n - 1) ~= '\\' then
            i = n + 1
            b.i = i
            return b.s:sub(j, n - 1)
        else return end
    end
end

function _parse_nstring(b)
    local i = b.i

    if b.s:sub(i, i) == '"' then
        i = i + 1
    elseif _parse_nil(b) then
        return 'NIL'
    else return end

    local j = i
    local n = 0
    while true do
        n = b.s:find('"', i + n)
        if not n then return end
        if b.s:sub(n - 1, n - 1) ~= '\\' then
            i = n + 1
            b.i = i
            return b.s:sub(j, n - 1)
        else return end
    end
end

function _parse_number(b)
    local j = b.i
    local n = b.s:find('[^0-9]', b.i)
    if not n then return end
    b.i = n
    return tonumber(b.s:sub(j, n - 1))
end
