-- The Mailbox class represents a mailbox that resides in an IMAP account.

Mailbox = {}

Mailbox._mt = {}
setmetatable(Mailbox, Mailbox._mt)


Mailbox._mt.__call = function (self, account, mailbox)
    local object = {}

    object._type = 'mailbox'

    for key, value in pairs(Mailbox) do
        if (type(value) == 'function') then
            object[key] = value
        end
    end

    object._mt = {}
    object._mt.__index = object._attach_message
    setmetatable(object, object._mt)

    object._account = account
    object._mailbox = mailbox

    return object
end


function Mailbox._attach_message(self, uid)
    self[uid] = Message(self._account, self, uid)
    return self[uid]
end

function Mailbox._detach_message(self, uid)
    self[uid] = nil
end


function Mailbox._cached_select(self, account_imap, mbox)
    -- Account._login_user sets Account._imap._mailbox to nil to force select
    -- to be re-run after (re)login
    -- account_imap == Account._imap
    if (account_imap._mailbox == nil or account_imap._mailbox ~= mbox) then
        if (ifcore.select(account_imap, mbox) == true) then
            account_imap._mailbox = mbox
            return true
        else
            return false
        end
    else
        return true
    end
end

function Mailbox._cached_close(self, account_imap)
    account_imap._mailbox = nil
    return ifcore.close(self._account._imap)
end


function Mailbox._send_query(self, criteria, charset)
    _check_optional(criteria, { 'string', 'table' })
    _check_optional(charset, 'string')

    local query
    if (criteria == nil) then
        query = 'ALL'
    elseif (type(criteria) == 'string') then
        query = 'ALL ' .. criteria
    else 
        query = _make_query(criteria)
    end

    if (charset == nil) then
        if (type(options) == 'table' and type(options.charset) == 'string') then
            charset = options.charset
        else
            charset = ''
        end
    end

    if (self._account._login_user(self._account) ~= true) then
        return {}
    end

    if (self._cached_select(self, self._account._imap, self._mailbox) ~=
        true) then
        return {}
    end
   
    local _, results = ifcore.search(self._account._imap, query, charset)

    if (type(options) == 'table' and options.close == true) then
        self._cached_close(self, self._account._imap)
    end

    if (results == nil) then
        return {}
    end

    local t = {}
    for n in string.gmatch(results, '%d+') do
        table.insert(t, { self, tonumber(n) })
    end

    return t
end


function Mailbox._flag_messages(self, mode, flags, messages)
    if (not messages or #messages == 0) then
        return
    end

    if (self._account._login_user(self._account) ~= true) then
        return
    end

    if (self._cached_select(self, self._account._imap, self._mailbox) ~=
        true) then
        return
    end

    local f = ''
    if (#flags ~= 0) then
        f = table.concat(flags, ' ')
    end

    local m = _make_range(messages)
    local n = #m
    local r = false
    for i = 1, n, 50 do
        j = i + 49
        if (n < j) then
            j = n
        end
        r = ifcore.store(self._account._imap, table.concat(m, ',', i, j),
            mode, f)
        if r == false then
            break
        end
    end

    if (type(options) == 'table' and options.close == true) then
        self._cached_close(self, self._account._imap)
    end

    return r
end


function Mailbox._copy_messages(self, dest, messages)
    if (not messages or #messages == 0) then
        return
    end

    if (self._account._login_user(self._account) ~= true) then
        return
    end

    local r = false
    if (self._account._imap == dest._account._imap) then
        if (self._cached_select(self, self._account._imap, self._mailbox) ~= true) then
            return
        end

        local m = _make_range(messages)
        local n = #m
        for i = 1, n, 50 do
            j = i + 49
            if (n < j) then
                j = n
            end
            r = ifcore.copy(self._account._imap, table.concat(m, ',', i, j),
                dest._mailbox)
            if r == false then
                break
            end
        end

        if (type(options) == 'table' and options.close == true) then
            self._cached_close(self, self._account._imap)
        end
    else
        local fast = self._fetch_fast(self, messages)
        local mesgs = self._fetch_message(self, messages)

        if (dest._account._login_user(dest._account) ~= true) then
            return
        end

        for i in pairs(fast) do
            for k, v in ipairs(fast[i]['flags']) do
                if (string.lower(v) == '\\recent') then
                    table.remove(fast[i]['flags'], k)
                end
            end

            r = ifcore.append(dest._account._imap, dest._mailbox, mesgs[i],
                table.concat(fast[i]['flags'], ' '), fast[i]['date'])
        end
    end

    return r
end


function Mailbox._fetch_fast(self, messages)
    if (not messages or #messages == 0) then
        return
    end

    if (self._account._login_user(self._account) ~= true) then
        return
    end

    if (self._cached_select(self, self._account._imap, self._mailbox) ~=
        true) then
        return
    end

    local results = {}
    for _, m in ipairs(messages) do
        local _, flags, date, size = ifcore.fetchfast(self._account._imap,
            tostring(m))
        if (flags ~= nil and date ~= nil and size ~= nil ) then
            local f = {}
            for s in string.gmatch(flags, '%S+') do
                table.insert(f, s)
            end
            results[m] = {}
            results[m]['flags'] = f
            results[m]['date'] = date
            results[m]['size'] = size
        end
    end

    if (type(options) == 'table' and options.close == true) then
        self._cached_close(self, self._account._imap)
    end

    return results
end

function Mailbox._fetch_flags(self, messages)
    if (not messages or #messages == 0) then
        return
    end

    if (self._account._login_user(self._account) ~= true) then
        return
    end

    if (self._cached_select(self, self._account._imap, self._mailbox) ~=
        true) then
        return
    end

    local results = {}
    for _, m in ipairs(messages) do
        local _, flags = ifcore.fetchflags(self._account._imap, tostring(m))
        if (flags ~= nil) then
            local f = {}
            for s in string.gmatch(flags, '%S+') do
                table.insert(f, s)
            end
            results[m] = f
        end
    end

    if (type(options) == 'table' and options.close == true) then
        self._cached_close(self, self._account._imap)
    end

    return results
end

function Mailbox._fetch_date(self, messages)
    if (not messages or #messages == 0) then
        return
    end

    if (self._account._login_user(self._account) ~= true) then
        return
    end

    if (self._cached_select(self, self._account._imap, self._mailbox) ~=
        true) then
        return
    end

    local results = {}
    for _, m in ipairs(messages) do
        if (type(options) == 'table' and options.cache == true and
            self[m]._date) then
            results[m] = self[m]._date
        else
            local _, date = ifcore.fetchdate(self._account._imap, tostring(m))
            if (date ~= nil) then
                results[m] = date
                if (type(options) == 'table' and options.cache == true) then
                    self[m]._date = date
                end
            end
        end
    end

    if (type(options) == 'table' and options.close == true) then
        self._cached_close(self, self._account._imap)
    end

    return results
end

function Mailbox._fetch_size(self, messages)
    if (not messages or #messages == 0) then
        return
    end

    if (self._account._login_user(self._account) ~= true) then
        return
    end

    if (self._cached_select(self, self._account._imap, self._mailbox) ~=
        true) then
        return
    end

    local results = {}
    for _, m in ipairs(messages) do
        if (type(options) == 'table' and options.cache == true and
            self[m]._size) then
            results[m] = self[m]._size
        else
            local _, size = ifcore.fetchsize(self._account._imap, tostring(m))
            if (size ~= nil) then
                results[m] = tonumber(size)
                if (type(options) == 'table' and options.cache == true) then
                    self[m]._size = tonumber(size)
                end
            end
        end
    end

    if (type(options) == 'table' and options.close == true) then
        self._cached_close(self, self._account._imap)
    end

    return results
end

function Mailbox._fetch_header(self, messages)
    if (not messages or #messages == 0) then
        return
    end

    if (self._account._login_user(self._account) ~= true) then
        return
    end

    if (self._cached_select(self, self._account._imap, self._mailbox) ~=
        true) then
        return
    end

    local results = {}
    for _, m in ipairs(messages) do
        if (type(options) == 'table' and options.cache == true and
            self[m]._header) then
            results[m] = self[m]._header
        else
            local _, header = ifcore.fetchheader(self._account._imap,
                tostring(m))
            if (header ~= nil) then
                results[m] = header
                if (type(options) == 'table' and options.cache == true) then
                    self[m]._header = header
                end
            end
        end
    end

    if (type(options) == 'table' and options.close == true) then
        self._cached_close(self, self._account._imap)
    end

    return results
end

function Mailbox._fetch_body(self, messages)
    if (not messages or #messages == 0) then
        return
    end

    if (self._account._login_user(self._account) ~= true) then
        return
    end

    if (self._cached_select(self, self._account._imap, self._mailbox) ~=
        true) then
        return
    end

    local results = {}
    for _, m in ipairs(messages) do
        if (type(options) == 'table' and options.cache == true and
            self[m]._body) then
            results[m] = self[m]._body
        else
            local _, body = ifcore.fetchbody(self._account._imap, tostring(m))
            if (body ~= nil) then
                results[m] = body
                if (type(options) == 'table' and options.cache == true) then
                    self[m]._body = body
                end
            end
        end
    end

    if (type(options) == 'table' and options.close == true) then
        self._cached_close(self, self._account._imap)
    end

    return results
end

function Mailbox._fetch_message(self, messages)
    if (not messages or #messages == 0) then
        return
    end

    local header = self._fetch_header(self, messages)
    local body = self._fetch_body(self, messages)

    local results = {}
    for _, m in ipairs(messages) do
        if (header[m] == nil) then
            results[m] = nil
        elseif (body[m] == nil) then
            results[m] = header[m]
        else
            results[m] = header[m] .. body[m]
        end
    end

    return results
end


function Mailbox._fetch_fields(self, fields, messages)
    if (not messages or #messages == 0) then
        return
    end

    if (self._account._login_user(self._account) ~= true) then
        return
    end

    if (self._cached_select(self, self._account._imap, self._mailbox) ~=
        true) then
        return
    end

    local results = {}
    for _, m in ipairs(messages) do
        results[m] = ''
        for _, f in ipairs(fields) do
            if (type(options) == 'table' and options.cache == true and
                self[m]._fields[f]) then
                results[m] = results[m] .. self[m]._fields[f]
            else
                local _, field = ifcore.fetchfields(self._account._imap,
                    tostring(m), f)
                if (field ~= nil) then
                    field = string.gsub(field, "\r\n\r\n$", "\n")
                    results[m] = results[m] .. field
                    if (type(options) == 'table' and options.cache == true) then
                        self[m]._fields[f] = field
                    end
                end
            end
        end
        results[m] = string.gsub(results[m], "\n$", "")
    end

    if (type(options) == 'table' and options.close == true) then
        self._cached_close(self, self._account._imap)
    end

    return results
end

function Mailbox._fetch_structure(self, messages)
    if (not messages or #messages == 0) then
        return
    end

    if (self._account._login_user(self._account) ~= true) then
        return
    end

    if (self._cached_select(self, self._account._imap, self._mailbox) ~=
        true) then
        return
    end

    local results = {}
    for _, m in ipairs(messages) do
        if (type(options) == 'table' and options.cache == true and
            self[m]._structure) then
            results[m] = self[m]._structure
        else
            local _, structure = ifcore.fetchstructure(self._account._imap,
                tostring(m))
            if (structure ~= nil) then
                local parsed = _parse_structure({ ['s'] = structure, ['i'] = 1 })
                results[m] = parsed
                if (type(options) == 'table' and options.cache == true) then
                    self[m]._structure = parsed
                end
            end
        end
    end

    if (type(options) == 'table' and options.close == true) then
        self._cached_close(self, self._account._imap)
    end

    return results
end

function Mailbox._fetch_parts(self, parts, message)
    if (self._account._login_user(self._account) ~= true) then
        return
    end

    if (self._cached_select(self, self._account._imap, self._mailbox) ~=
        true) then
        return
    end

    local results = {}
    for _, part in ipairs(parts) do
        results[part] = ''
        if (type(options) == 'table' and options.cache == true and
            self[message]._parts[part]) then
            results[part] = self[message]._parts[part]
        else
            local _, bodypart = ifcore.fetchpart(self._account._imap,
                tostring(message), part)
            if (bodypart ~= nil) then
                results[part] = bodypart
                self[message]._parts[part] = bodypart
            end
        end
    end

    if (type(options) == 'table' and options.close == true) then
        self._cached_close(self, self._account._imap)
    end

    return results
end


function Mailbox.check_status(self)
    if (self._account._login_user(self._account) ~= true) then
        return
    end

    local _, exist, recent, unseen, uidnext = ifcore.status(self._account._imap,
        self._mailbox)

    if (type(options) == 'table' and options.info == true) then
        print(string.format("%d messages, %d recent, %d unseen, in %s@%s/%s.",
                            exist, recent, unseen,
                            self._account._imap.username,
                            self._account._imap.server, self._mailbox))
    end

    return exist, recent, unseen, uidnext
end


function Mailbox.send_query(self, criteria, charset)
    return Set(self._send_query(self, criteria, charset))
end

function Mailbox.select_all(self)
    return self.send_query(self)
end


function Mailbox.add_flags(self, flags, messages)
    _check_required(flags, 'table')
    _check_required(messages, 'table')
    
    local mesgs = _extract_messages(self, messages)

    local r = self._flag_messages(self, 'add', flags, mesgs)

    if (type(options) == 'table' and options.info == true and r == true) then
        print(string.format("%d messages flagged in %s@%s/%s.",
                            #mesgs, self._account._imap.username,
                            self._account._imap.server, self._mailbox))
    end

    return r
end

function Mailbox.remove_flags(self, flags, messages)
    _check_required(flags, 'table')
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)

    local r = self._flag_messages(self, 'remove', flags, mesgs)

    if (type(options) == 'table' and options.info == true and r == true) then
        print(string.format("%d messages flagged in %s@%s/%s.",
                            #mesgs, self._account._imap.username,
                            self._account._imap.server, self._mailbox))
    end

    return r
end

function Mailbox.replace_flags(self, flags, messages)
    _check_required(flags, 'table')
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)

    local r = self._flag_messages(self, 'replace', flags, mesgs)

    if (type(options) == 'table' and options.info == true and r == true) then
        print(string.format("%d messages flagged in %s@%s/%s.",
                            #mesgs, self._account._imap.username,
                            self._account._imap.server, self._mailbox))
    end

    return r
end


function Mailbox.mark_answered(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)

    local r = self._flag_messages(self, 'add', { '\\Answered' }, mesgs)

    if (type(options) == 'table' and options.info == true and r == true) then
        print(string.format("%d messages marked answered in %s@%s/%s.",
                            #mesgs, self._account._imap.username,
                            self._account._imap.server, self._mailbox))
    end

    return r
end


function Mailbox.mark_deleted(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)

    local r = self._flag_messages(self, 'add', { '\\Deleted' }, mesgs)

    if (type(options) == 'table' and options.info == true and r == true) then
        print(string.format("%d messages marked deleted in %s@%s/%s.",
                            #mesgs, self._account._imap.username,
                            self._account._imap.server, self._mailbox))
    end

    return r
end

function Mailbox.mark_draft(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)

    local r = self._flag_messages(self, 'add', { '\\Draft' }, mesgs)

    if (type(options) == 'table' and options.info == true and r == true) then
        print(string.format("%d messages marked draft in %s@%s/%s.",
                            #mesgs, self._account._imap.username,
                            self._account._imap.server, self._mailbox))
    end

    return r
end

function Mailbox.mark_flagged(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)

    local r = self._flag_messages(self, 'add', { '\\Flagged' }, mesgs)

    if (type(options) == 'table' and options.info == true and r == true) then
        print(string.format("%d messages marked flagged in %s@%s/%s.",
                            #mesgs, self._account._imap.username,
                            self._account._imap.server, self._mailbox))
    end

    return r
end

function Mailbox.mark_seen(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)

    local r = self._flag_messages(self, 'add', { '\\Seen' }, mesgs)

    if (type(options) == 'table' and options.info == true and r == true) then
        print(string.format("%d messages marked seen in %s@%s/%s.",
                            #mesgs, self._account._imap.username,
                            self._account._imap.server, self._mailbox))
    end

    return r
end

function Mailbox.unmark_answered(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)

    local r = self._flag_messages(self, 'remove', { '\\Answered' }, mesgs)

    if (type(options) == 'table' and options.info == true and r == true) then
        print(string.format("%d messages unmarked answered in %s@%s/%s.",
                            #mesgs, self._account._imap.username,
                            self._account._imap.server, self._mailbox))
    end

    return r
end

function Mailbox.unmark_deleted(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)

    local r = self._flag_messages(self, 'remove', { '\\Deleted' }, mesgs)

    if (type(options) == 'table' and options.info == true and r == true) then
        print(string.format("%d messages unmarked deleted in %s@%s/%s.",
                            #mesgs, self._account._imap.username,
                            self._account._imap.server, self._mailbox))
    end

    return r
end

function Mailbox.unmark_draft(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)

    local r = self._flag_messages(self, 'remove', { '\\Draft' }, mesgs)

    if (type(options) == 'table' and options.info == true and r == true) then
        print(string.format("%d messages unmarked draft in %s@%s/%s.",
                            #mesgs, self._account._imap.username,
                            self._account._imap.server, self._mailbox))
    end

    return r
end

function Mailbox.unmark_flagged(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)

    local r = self._flag_messages(self, 'remove', { '\\Flagged' }, mesgs)

    if (type(options) == 'table' and options.info == true and r == true) then
        print(string.format("%d messages unmarked flagged in %s@%s/%s.",
                            #mesgs, self._account._imap.username,
                            self._account._imap.server, self._mailbox))
    end

    return r
end

function Mailbox.unmark_seen(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)

    local r = self._flag_messages(self, 'remove', { '\\Seen' }, mesgs)

    if (type(options) == 'table' and options.info == true and r == true) then
        print(string.format("%d messages unmarked seen in %s@%s/%s.",
                            #mesgs, self._account._imap.username,
                            self._account._imap.server, self._mailbox))
    end

    return r
end


function Mailbox.delete_messages(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)

    local r = self._flag_messages(self, 'add', { '\\Deleted' }, mesgs)

    if (type(options) == 'table' and options.info == true and r == true) then
        print(string.format("%d messages deleted in %s@%s/%s.",
                            #mesgs, self._account._imap.username,
                            self._account._imap.server, self._mailbox))
    end

    return r
end


function Mailbox.copy_messages(self, dest, messages)
    _check_required(dest, 'table')
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)

    local r = self._copy_messages(self, dest, mesgs)

    if (type(options) == 'table' and options.info == true and r == true) then
        print(string.format("%d messages copied from %s@%s/%s to %s@%s/%s.",
                            #mesgs, self._account._imap.username,
                            self._account._imap.server, self._mailbox,
                            dest._account._imap.username,
                            dest._account._imap.server,
                            dest._mailbox))
    end

    return r
end


function Mailbox.move_messages(self, dest, messages)
    _check_required(dest, 'table')
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)

    local rc = self._copy_messages(self, dest, mesgs)
    local rf = false
    if (rc == true) then
        rf = self._flag_messages(self, 'add', { '\\Deleted' }, mesgs)
    end

    if (type(options) == 'table' and options.info == true and
        rc == true and rf == true) then
        print(string.format("%d messages moved from %s@%s/%s to %s@%s/%s.",
                            #mesgs, self._account._imap.username,
                            self._account._imap.server, self._mailbox,
                            dest._account._imap.username,
                            dest._account._imap.server,
                            dest._mailbox))
    end

    return rc == true and rf == true
end


function Mailbox.fetch_flags(self, messages)
    _check_required(messages, 'table')

    return self._fetch_flags(self, _extract_messages(self, messages))
end

function Mailbox.fetch_date(self, messages)
    _check_required(messages, 'table')

    return self._fetch_date(self, _extract_messages(self, messages))
end

function Mailbox.fetch_size(self, messages)
    _check_required(messages, 'table')

    return self._fetch_size(self, _extract_messages(self, messages))
end

function Mailbox.fetch_header(self, messages)
    _check_required(messages, 'table')

    return self._fetch_header(self, _extract_messages(self, messages))
end

function Mailbox.fetch_body(self, messages)
    _check_required(messages, 'table')

    return self._fetch_body(self, _extract_messages(self, messages))
end

function Mailbox.fetch_message(self, messages)
    _check_required(messages, 'table')

    return self._fetch_message(self, _extract_messages(self, messages))
end

function Mailbox.fetch_fields(self, fields, messages)
    _check_required(fields, 'table')
    _check_required(messages, 'table')

    return self._fetch_fields(self, fields, _extract_messages(self, messages))
end

function Mailbox.fetch_structure(self, messages)
    _check_required(messages, 'table')

    return self._fetch_structure(self, _extract_messages(self, messages))
end

function Mailbox.fetch_parts(self, parts, message)
    _check_required(parts, 'table')
    _check_required(message, 'number')

    return self._fetch_parts(self, parts, message)
end


function Mailbox.is_answered(self)
    return self.send_query(self, 'ANSWERED')
end

function Mailbox.is_deleted(self)
    return self.send_query(self, 'DELETED')
end

function Mailbox.is_draft(self)
    return self.send_query(self, 'DRAFT')
end

function Mailbox.is_flagged(self)
    return self.send_query(self, 'FLAGGED')
end

function Mailbox.is_new(self)
    return self.send_query(self, 'NEW')
end

function Mailbox.is_old(self)
    return self.send_query(self, 'OLD')
end

function Mailbox.is_recent(self)
    return self.send_query(self, 'RECENT')
end

function Mailbox.is_seen(self)
    return self.send_query(self, 'SEEN')
end

function Mailbox.is_unanswered(self)
    return self.send_query(self, 'UNANSWERED')
end

function Mailbox.is_undeleted(self)
    return self.send_query(self, 'UNDELETED')
end

function Mailbox.is_undraft(self)
    return self.send_query(self, 'UNDRAFT')
end

function Mailbox.is_unflagged(self)
    return self.send_query(self, 'UNFLAGGED')
end

function Mailbox.is_unseen(self)
    return self.send_query(self, 'UNSEEN')
end

function Mailbox.is_larger(self, size)
    _check_required(size, 'number')

    return self.send_query(self, 'LARGER ' .. tostring(size))
end

function Mailbox.is_smaller(self, size)
    _check_required(size, 'number')

    return self.send_query(self, 'SMALLER ' .. tostring(size))
end


function Mailbox.arrived_on(self, date)
    _check_required(date, 'string')

    return self.send_query(self, 'ON ' .. date)
end

function Mailbox.arrived_before(self, date)
    _check_required(date, 'string')

    return self.send_query(self, 'BEFORE ' .. date)
end

function Mailbox.arrived_since(self, date)
    _check_required(date, 'string')

    return self.send_query(self, 'SINCE ' .. date)
end

function Mailbox.sent_on(self, date)
    _check_required(date, 'string')

    return self.send_query(self, 'SENTON ' .. date)
end

function Mailbox.sent_before(self, date)
    _check_required(date, 'string')

    return self.send_query(self, 'SENTBEFORE ' .. date)
end

function Mailbox.sent_since(self, date)
    _check_required(date, 'string')

    return self.send_query(self, 'SENTSINCE ' .. date)
end

function Mailbox.is_newer(self, days)
    _check_required(days, 'number')

    return self.send_query(self, 'SINCE ' .. form_date(days))
end

function Mailbox.is_older(self, days)
    _check_required(days, 'number')

    return self.send_query(self, 'BEFORE ' .. form_date(days))
end


function Mailbox.has_flag(self, flag)
    _check_required(flag, 'string')

    return self.send_query(self, 'KEYWORD "' .. flag .. '"')
end


function Mailbox.contain_field(self, field, string)
    _check_required(field, 'string')
    _check_required(string, 'string')

    return self.send_query(self, 'HEADER ' .. field .. ' "' .. string .. '"')
end

function Mailbox.contain_bcc(self, string)
    _check_required(string, 'string')

    return self.send_query(self, 'BCC "' .. string .. '"')
end

function Mailbox.contain_cc(self, string)
    _check_required(string, 'string')

    return self.send_query(self, 'CC "' .. string .. '"')
end

function Mailbox.contain_from(self, string)
    _check_required(string, 'string')

    return self.send_query(self, 'FROM "' .. string .. '"')
end

function Mailbox.contain_subject(self, string)
    _check_required(string, 'string')

    return self.send_query(self, 'SUBJECT "' .. string .. '"')
end

function Mailbox.contain_to(self, string)
    _check_required(string, 'string')

    return self.send_query(self, 'TO "' .. string .. '"')
end

function Mailbox.contain_header(self, string)
    _check_required(string, 'string')

    return self.send_query(self, 'TEXT "' .. string .. '" NOT BODY "' ..
        string .. '"')
end

function Mailbox.contain_body(self, string)
    _check_required(string, 'string')

    return self.send_query(self, 'BODY "' .. string .. '"')
end

function Mailbox.contain_message(self, string)
    _check_required(string, 'string')

    return self.send_query(self, 'TEXT "' .. string .. '"')
end


function Mailbox.match_field(self, field, pattern, messages)
    _check_required(field, 'string')
    _check_required(pattern, 'string')

    if not messages then
        messages = self._send_query(self)
    end
    local mesgs = _extract_messages(self, messages)
    local fields = self._fetch_fields(self, { field }, mesgs)

    if (#mesgs == 0 or fields == nil) then
        return Set({})
    end
 
    local results = {}
    for m, f in pairs(fields) do
        if (regex_search(pattern, f)) then
            table.insert(results, {self, m})
        end
    end

    return Set(results)
end

function Mailbox.match_bcc(self, pattern, messages)
    _check_required(pattern, 'string')

    return self.match_field(self, 'Bcc', pattern, messages)
end

function Mailbox.match_cc(self, pattern, messages)
    _check_required(pattern, 'string')

    return self.match_field(self, 'Cc', pattern, messages)
end

function Mailbox.match_from(self, pattern, messages)
    _check_required(pattern, 'string')

    return self.match_field(self, 'From', pattern, messages)
end

function Mailbox.match_subject(self, pattern, messages)
    _check_required(pattern, 'string')

    return self.match_field(self, 'Subject', pattern, messages)
end

function Mailbox.match_to(self, pattern, messages)
    _check_required(pattern, 'string')

    return self.match_field(self, 'To', pattern, messages)
end

function Mailbox.match_header(self, pattern, messages)
    _check_required(pattern, 'string')

    if not messages then
        messages = self._send_query(self)
    end
    local mesgs = _extract_messages(self, messages)
    local header = self._fetch_header(self, mesgs)

    if (#mesgs == 0 or header == nil) then
        return Set({})
    end

    local results = {}
    for m, h in pairs(header) do
        if (regex_search(pattern, h)) then
            table.insert(results, {self, m})
        end
    end

    return Set(results)
end

function Mailbox.match_body(self, pattern, messages)
    _check_required(pattern, 'string')

    if not messages then
        messages = self._send_query(self)
    end
    local mesgs = _extract_messages(self, messages)
    local body = self._fetch_body(self, mesgs)

    if (#mesgs == 0 or body == nil) then
        return Set({})
    end

    local results = {}
    for m, b in pairs(body) do
        if (regex_search(pattern, b)) then
            table.insert(results, {self, m})
        end
    end

    return Set(results)
end

function Mailbox.match_message(self, pattern, messages)
    _check_required(pattern, 'string')

    if not messages then
        messages = self._send_query(self)
    end
    local mesgs = _extract_messages(self, messages)
    local full = self._fetch_message(self, mesgs)

    if (#mesgs == 0 or full == nil) then
        return Set({})
    end

    local results = {}
    for m, f in pairs(full) do
        if (regex_search(pattern, f)) then
            table.insert(results, {self, m})
        end
    end

    return Set(results)
end

function Mailbox.enter_idle(self)
    if (self._account._login_user(self._account) ~= true) then
        return false
    end

    if (self._cached_select(self, self._account._imap, self._mailbox) ~=
        true) then
        return false
    end
   
    return ifcore.idle(self._account._imap)
end

Mailbox._mt.__index = function () end
Mailbox._mt.__newindex = function () end
