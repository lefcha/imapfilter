-- Compatibility workarounds and the old obsolete interface

if _VERSION == 'Lua 5.1' then
    table.unpack = unpack
else
    unpack = table.unpack
end


function check(account, mbox)
    _check_required(account, 'table')
    _check_required(mbox, 'string')

    _check_required(account.server, 'string')
    _check_required(account.username, 'string')
    _check_required(account.password, 'string')

    if ifcore.login(account) ~= true then
        return
    end

    local _, exist, recent, unseen = ifcore.status(account, mbox)

    if type(options) == 'table' and options.info == true then
        print(string.format("%d messages, %d recent, %d unseen, in %s@%s/%s.",
                            exist, recent, unseen, account.username,
                            account.server, mbox))
    end

    return exist, recent, unseen
end


function match(account, mbox, criteria)
    _check_required(account, 'table')
    _check_required(mbox, 'string')
    _check_required(criteria, 'table')

    _check_required(account.server, 'string')
    _check_required(account.username, 'string')
    _check_required(account.password, 'string')

    if ifcore.login(account) ~= true then
        return
    end

    if _cached_select(account, mbox) ~= true then
        return {}
    end

    local charset = ''
    if type(options) == 'table' and type(options.charset) == 'string' then
        charset = options.charset
    end

    local _, results = ifcore.search(account, _make_query(criteria), charset)

    if type(options) == 'table' and options.close == true then
        _cached_close(account)
    end

    if results == nil then
        return {}
    end

    local t = {}
    for n in string.gmatch(results, '%d+') do
        table.insert(t, tonumber(n))
    end

    return t
end


function flag(account, mbox, mode, flags, messages)
    _check_required(account, 'table')
    _check_required(mbox, 'string')
    _check_required(mode, 'string')
    _check_required(flags, 'table')
    _check_required(messages, 'table')

    _check_required(account.server, 'string')
    _check_required(account.username, 'string')
    _check_required(account.password, 'string')

    local r = flag_aux(account, mbox, mode, flags, messages)

    if type(options) == 'table' and options.info == true and
            messages ~= nil and r == true then
        print(string.format("%d messages flagged in %s@%s/%s.",
                            #messages, account.username,
                            account.server, mbox))
    end

    return r
end

function flag_aux(account, mbox, mode, flags, messages)
    if #messages == 0 then
        return
    end

    if mode ~= 'add' and mode ~= 'remove' and mode ~= 'replace' then
        error('"add", "remove" or "replace" expected for mode', 3)
    end


    if ifcore.login(account) ~= true then
        return
    end

    if _cached_select(account, mbox) ~= true then
        return
    end

    local f = ''
    if #flags ~= 0 then
        if flags.keywords ~= true then
            for k, v in ipairs(flags) do
                if string.lower(v) == 'answered' or
                    string.lower(v) == 'deleted' or
                    string.lower(v) == 'draft' or
                    string.lower(v) == 'flagged' or
                    string.lower(v) == 'seen' then
                    f = f .. '\\' .. v .. ' '
                end
            end
            f = string.gsub(f, '(.+) ', '%1')
        else
            f = table.concat(flags, ' ')
        end
    end

    local m = _make_range(messages)
    n = #m
    local r = false
    for i = 1, n, 50 do
        j = i + 49
        if n < j then
            j = n
        end
        r = ifcore.store(account, table.concat(m, ',', i, j), mode, f)
        if r == false then
            break
        end
    end

    if type(options) == 'table' and options.close == true then
        _cached_close(account)
    end

    return r
end


function copy(srcaccount, srcmbox, dstaccount, dstmbox, messages)
    _check_required(srcaccount, 'table')
    _check_required(srcmbox, 'string')
    _check_required(dstaccount, 'table')
    _check_required(dstmbox, 'string')
    _check_required(messages, 'table')

    _check_required(srcaccount.server, 'string')
    _check_required(srcaccount.username, 'string')
    _check_required(srcaccount.password, 'string')

    _check_required(dstaccount.server, 'string')
    _check_required(dstaccount.username, 'string')
    _check_required(dstaccount.password, 'string')

    local r = copy_aux(srcaccount, srcmbox, dstaccount, dstmbox, messages)

    if type(options) == 'table' and options.info == true and
            messages ~= nil and r == true then
        print(string.format("%d messages copied from %s@%s/%s to %s@%s/%s.",
                            #messages, srcaccount.username,
                            srcaccount.server, srcmbox, dstaccount.username,
                            dstaccount.server, dstmbox))
    end

    return r
end

function copy_aux(srcaccount, srcmbox, dstaccount, dstmbox, messages)
    if #messages == 0 then
        return
    end

    if ifcore.login(srcaccount) ~= true then
        return
    end

    local r = false
    if srcaccount == dstaccount then
        if _cached_select(srcaccount, srcmbox) ~= true then
            return
        end

        local m = _make_range(messages)
        n = #m
        for i = 1, n, 50 do
            j = i + 49
            if n < j then
                j = n
            end
            r = ifcore.copy(srcaccount, table.concat(m, ',', i, j), dstmbox)
            if r == false then
                break
            end
        end

        if type(options) == 'table' and options.close == true then
            _cached_close(srcaccount)
        end
    else
        local fast = fetchfast(srcaccount, srcmbox, messages)
        local msgs = fetchmessage(srcaccount, srcmbox, messages)

        if ifcore.login(dstaccount) ~= true then
            return
        end

        for i in pairs(fast) do
            for k, v in ipairs(fast[i]['flags']) do
                if string.lower(v) == '\\recent' then
                    table.remove(fast[i]['flags'], k)
                end
            end

            r = ifcore.append(dstaccount, dstmbox, msgs[i],
                table.concat(fast[i]['flags'], ' '), fast[i]['date'])
        end
    end

    return r
end


function delete(account, mbox, messages)
    _check_required(account, 'table')
    _check_required(mbox, 'string')
    _check_required(messages, 'table')

    _check_required(account.server, 'string')
    _check_required(account.username, 'string')
    _check_required(account.password, 'string')

    local r = flag_aux(account, mbox, 'add', { 'Deleted' }, messages)

    if type(options) == 'table' and options.info == true and
            messages ~= nil and r == true then
        print(string.format("%d messages deleted in %s@%s/%s.",
                            #messages, account.username,
                            account.server, mbox))
    end

    return r
end


function move(srcaccount, srcmbox, dstaccount, dstmbox, messages)
    _check_required(srcaccount, 'table')
    _check_required(srcmbox, 'string')
    _check_required(dstaccount, 'table')
    _check_required(dstmbox, 'string')
    _check_required(messages, 'table')

    _check_required(srcaccount.server, 'string')
    _check_required(srcaccount.username, 'string')
    _check_required(srcaccount.password, 'string')

    _check_required(dstaccount.server, 'string')
    _check_required(dstaccount.username, 'string')
    _check_required(dstaccount.password, 'string')

    local rc = copy_aux(srcaccount, srcmbox, dstaccount, dstmbox, messages)
    local rf = false
    if rc == true then
        rf = flag_aux(srcaccount, srcmbox, 'add', { 'Deleted' }, messages)
    end

    if type(options) == 'table' and options.info == true and
            messages ~= nil and rc == true and rf == true then
        print(string.format("%d messages moved from %s@%s/%s to %s@%s/%s.",
                            #messages, srcaccount.username,
                            srcaccount.server, srcmbox, dstaccount.username,
                            dstaccount.server, dstmbox))
    end

    return rc == true and rf == true
end


function fetchheader(account, mbox, messages)
    _check_required(account, 'table')
    _check_required(mbox, 'string')
    _check_required(messages, 'table')

    _check_required(account.server, 'string')
    _check_required(account.username, 'string')
    _check_required(account.password, 'string')

    if #messages == 0 then
        return
    end

    if ifcore.login(account) ~= true then
        return
    end

    if _cached_select(account, mbox) ~= true then
        return
    end

    local results = {}
    for i, v in ipairs(messages) do
        local _, header = ifcore.fetchheader(account, tostring(v))

        if header ~= nil then
            results[tonumber(v)] = header
        end
    end

    if type(options) == 'table' and options.close == true then
        _cached_close(account)
    end

    return results
end

function fetchbody(account, mbox, messages)
    _check_required(account, 'table')
    _check_required(mbox, 'string')
    _check_required(messages, 'table')

    _check_required(account.server, 'string')
    _check_required(account.username, 'string')
    _check_required(account.password, 'string')

    if #messages == 0 then
        return
    end

    if ifcore.login(account) ~= true then
        return
    end

    if _cached_select(account, mbox) ~= true then
        return
    end

    local results = {}
    for i, v in ipairs(messages) do
        local _, body = ifcore.fetchbody(account, tostring(v))

        if body ~= nil then
            results[tonumber(v)] = body
        end
    end

    if type(options) == 'table' and options.close == true then
        _cached_close(account)
    end

    return results
end

function fetchmessage(account, mbox, messages)
    _check_required(account, 'table')
    _check_required(mbox, 'string')
    _check_required(messages, 'table')

    _check_required(account.server, 'string')
    _check_required(account.username, 'string')
    _check_required(account.password, 'string')

    if #messages == 0 then
        return
    end

    if ifcore.login(account) ~= true then
        return
    end

    if _cached_select(account, mbox) ~= true then
        return
    end

    local results = {}
    for i, v in ipairs(messages) do
        local _, header = ifcore.fetchheader(account, tostring(v))
        local _, body = ifcore.fetchbody(account, tostring(v))

        if header ~= nil and body ~= nil then
            results[tonumber(v)] = header .. body
        end
    end

    if type(options) == 'table' and options.close == true then
        _cached_close(account)
    end

    return results
end

fetchtext = fetchmessage

function fetchfields(account, mbox, fields, messages)
    _check_required(account, 'table')
    _check_required(mbox, 'string')
    _check_required(fields, 'table')
    _check_required(messages, 'table')

    _check_required(account.server, 'string')
    _check_required(account.username, 'string')
    _check_required(account.password, 'string')

    if #messages == 0 then
        return
    end

    if ifcore.login(account) ~= true then
        return
    end

    if _cached_select(account, mbox) ~= true then
        return
    end

    local results = {}
    for i, v in ipairs(messages) do
        local _, headerfields = ifcore.fetchfields(account, tostring(v),
                                               table.concat(fields, ' '))

        if headerfields ~= nil then
            results[tonumber(v)] = headerfields
        end
    end

    if type(options) == 'table' and options.close == true then
        _cached_close(account)
    end

    return results
end

fetchheaders = fetchfields

function fetchfast(account, mbox, messages)
    _check_required(account, 'table')
    _check_required(mbox, 'string')
    _check_required(messages, 'table')

    _check_required(account.server, 'string')
    _check_required(account.username, 'string')
    _check_required(account.password, 'string')

    if #messages == 0 then
        return
    end

    if ifcore.login(account) ~= true then
        return
    end

    if _cached_select(account, mbox) ~= true then
        return
    end

    local results = {}
    for i, v in ipairs(messages) do
        local _, flags, date, size = ifcore.fetchfast(account, tostring(v))
        if flags ~= nil and date ~= nil and size ~= nil  then
            local f = {}
            for s in string.gmatch(flags, '%S+') do
                table.insert(f, s)
            end
            results[tonumber(v)] = {}
            results[tonumber(v)]['flags'] = f
            results[tonumber(v)]['date'] = date
            results[tonumber(v)]['size'] = size
        end
    end

    if type(options) == 'table' and options.close == true then
        _cached_close(account)
    end

    return results
end


function list(account, name)
    _check_required(account, 'table')
    _check_optional(name, 'string')

    _check_required(account.server, 'string')
    _check_required(account.username, 'string')
    _check_required(account.password, 'string')

    if name == nil then
        name = ''
    else
        if type(options) == 'table' and options.namespace == true then
            if name == '/' then
                name = ''
            end
            if name ~= '' then
                name = name .. '/'
            end
        end
    end

    if ifcore.login(account) ~= true then
        return
    end

    local _, mboxs, folders = ifcore.list(account, '', name .. '%')

    local m = {}
    for s in string.gmatch(mboxs, '%C+') do
        table.insert(m, s)
    end

    local f = {}
    for s in string.gmatch(folders, '%C+') do
        if s ~= name and s ~= name .. '/' then
            table.insert(f, s)
        end
    end

    return m, f
end

function lsub(account, name)
    _check_required(account, 'table')
    _check_optional(name, 'string')

    _check_required(account.server, 'string')
    _check_required(account.username, 'string')
    _check_required(account.password, 'string')

    if name == nil then
        name = ''
    else
        if type(options) == 'table' and options.namespace == true then
            if name == '/' then
                name = ''
            end
            if name ~= '' then
                name = name .. '/'
            end
        end
    end

    if ifcore.login(account) ~= true then
        return
    end

    local _, mboxs, folders = ifcore.lsub(account, '', name .. '%')

    local m = {}
    for s in string.gmatch(mboxs, '%C+') do
        table.insert(m, s)
    end

    local f = {}
    for s in string.gmatch(folders, '%C+') do
        if s ~= name and s ~= name .. '/' then
            table.insert(f, s)
        end
    end

    return m, f
end


function ping(account)
    _check_required(account, 'table')

    _check_required(account.server, 'string')
    _check_required(account.username, 'string')
    _check_required(account.password, 'string')

    if ifcore.login(account) ~= true then
        return
    end

    local r = ifcore.noop(account)

    return r
end


function _cached_select(account, mbox)
    if account.mailbox == nil or account.mailbox ~= mbox then
        if ifcore.select(account, mbox) == true then
            account.mailbox = mbox
            return true
        else
            return false
        end
    else
        return true
    end
end

function _cached_close(account)
    account.mailbox = nil
    return ifcore.close(account)
end


date_before = form_date
get_pass = get_password
daemon_mode = become_daemon
