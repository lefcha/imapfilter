-- The Mailbox class represents a mailbox that resides in an IMAP account.

Mailbox = {}

Mailbox._mt = {}
setmetatable(Mailbox, Mailbox._mt)


Mailbox._mt.__call = function (self, account, mailbox)
    local object = {}

    object._type = 'mailbox'
    object._account = account
    object._mailbox = mailbox

    for key, value in pairs(Mailbox) do
        if type(value) == 'function' then object[key] = value end
    end

    object._mt = {}
    object._mt.__index = object._attach_message
    setmetatable(object, object._mt)

    return object
end


function Mailbox._get_details(self)
    return self._account._account.username .. '@' ..
           self._account._account.server .. '/' .. self._mailbox
end

function Mailbox._check_connection(self)
    if not self._account._account.session then
        error('not connected to ' ..
              self._account._get_details(self._account), 0)
    end
end

function Mailbox._check_result(self, request, result)
    if result == nil then
        self._account._account.session = nil
        self._account._account.selected = nil
        error(request .. ' request to ' ..
              self._account._get_details(self._account) .. 'failed', 0)
    end
end


function Mailbox._attach_message(self, uid)
    self[uid] = Message(self._account, self, uid)
    return self[uid]
end

function Mailbox._detach_message(self, uid)
    self[uid] = nil
end


function Mailbox._cached_select(self)
    if self._account._account.selected == nil or
        self._account._account.selected ~= self._mailbox then

        self._check_connection(self)
        local r = ifcore.select(self._account._account.session, self._mailbox)
        self._check_result(self, 'select', r)
        if r == false then return false end

        self._account._account.selected = self._mailbox
    end
    return true
end

function Mailbox._cached_close(self)
    self._check_connection(self)
    local r = ifcore.close(self._account._account.session)
    self._check_result(self, 'close', r)
    if r == false then return false end

    self._account._account.selected = nil

    return true
end


function Mailbox._send_query(self, criteria, charset)
    _check_optional(criteria, { 'string', 'table' })
    _check_optional(charset, 'string')

    if self._cached_select(self) ~= true then return {} end

    local query
    if criteria == nil then
        query = 'ALL'
    elseif type(criteria) == 'string' then
        query = 'ALL ' .. criteria
    else 
        query = _make_query(criteria)
    end

    if charset == nil then
        if type(options.charset) == 'string' then
            charset = options.charset
        else
            charset = ''
        end
    end

    self._check_connection(self)
    local r, results = ifcore.search(self._account._account.session, query,
                                     charset)
    self._check_result(self, 'search', r)
    if r == false then return false end

    if options.close == true then self._cached_close(self) end
    if results == nil then return {} end

    local t = {}
    for n in string.gmatch(results, '%d+') do
        table.insert(t, { self, tonumber(n) })
    end

    return t
end


function Mailbox._flag_messages(self, mode, flags, messages)
    if not messages or #messages == 0 then return end
    if self._cached_select(self) ~= true then return end

    local f = ''
    if #flags ~= 0 then f = table.concat(flags, ' ') end
    local m = _make_range(messages)
    local n = #m
    local r = false
    for i = 1, n, 50 do
        j = i + 49
        if n < j then
            j = n
        end
        self._check_connection(self)
        r = ifcore.store(self._account._account.session, table.concat(m, ',',
                         i, j), mode, f)
        self._check_result(self, 'store', r)
        if r == false then break end
    end

    if options.close == true then self._cached_close(self) end

    return true
end


function Mailbox._copy_messages(self, dest, messages)
    if not messages or #messages == 0 then return end

    local r = false
    if self._account._account.session == dest._account._account.session then
        if self._cached_select(self) ~= true then return end

        local m = _make_range(messages)
        local n = #m
        for i = 1, n, 50 do
            j = i + 49
            if n < j then
                j = n
            end
            self._check_connection(self)
            r = ifcore.copy(self._account._account.session,
                            table.concat(m, ',', i, j), dest._mailbox)
            self._check_result(self, 'copy', r)
            if r == false then break end
        end

        if options.close == true then self._cached_close(self) end
    else
        local fast = self._fetch_fast(self, messages)
        local mesgs = self._fetch_message(self, messages)

        for i in pairs(fast) do
            for k, v in ipairs(fast[i]['flags']) do
                if string.lower(v) == '\\recent' then
                    table.remove(fast[i]['flags'], k)
                end
            end

            self._check_connection(dest)
            r = ifcore.append(dest._account._account.session, dest._mailbox,
                              mesgs[i], table.concat(fast[i]['flags'], ' '),
                              fast[i]['date'])
            self._check_result(dest, 'append', r)
            if r == false then break end
        end
    end

    return true
end


function Mailbox._fetch_fast(self, messages)
    if not messages or #messages == 0 then return end
    if self._cached_select(self) ~= true then return end

    local results = {}
    for _, m in ipairs(messages) do
        self._check_connection(self)
        local r, flags, date, size =
            ifcore.fetchfast(self._account._account.session, tostring(m))
        self._check_result(self, 'fetchfast', r)
        if r == false then break end

        if flags ~= nil and date ~= nil and size ~= nil  then
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

    if options.close == true then self._cached_close(self) end

    return results
end

function Mailbox._fetch_flags(self, messages)
    if not messages or #messages == 0 then return end
    if self._cached_select(self) ~= true then return end

    local results = {}
    for _, m in ipairs(messages) do
        self._check_connection(self)
        local r, flags = ifcore.fetchflags(self._account._account.session,
                                           tostring(m))
        self._check_result(self, 'fetchfast', r)
        if r == false then break end

        if flags ~= nil then
            local f = {}
            for s in string.gmatch(flags, '%S+') do
                table.insert(f, s)
            end
            results[m] = f
        end
    end

    if options.close == true then self._cached_close(self) end

    return results
end

function Mailbox._fetch_date(self, messages)
    if not messages or #messages == 0 then return end
    if self._cached_select(self) ~= true then return end

    local results = {}
    for _, m in ipairs(messages) do
        if options.cache == true and
            self[m]._date then
            results[m] = self[m]._date
        else
            self._check_connection(self)
            local r, date = ifcore.fetchdate(self._account._account.session,
                                             tostring(m))
            self._check_result(self, 'fetchdate', r)
            if r == false then break end

            if date ~= nil then
                results[m] = date
                if options.cache == true then self[m]._date = date end
            end
        end
    end

    if options.close == true then self._cached_close(self) end

    return results
end

function Mailbox._fetch_size(self, messages)
    if not messages or #messages == 0 then return end
    if self._cached_select(self) ~= true then return end

    local results = {}
    for _, m in ipairs(messages) do
        if options.cache == true and
            self[m]._size then
            results[m] = self[m]._size
        else
            self._check_connection(self)
            local r, size = ifcore.fetchsize(self._account._account.session,
                                             tostring(m))
            self._check_result(self, 'fetchsize', r)
            if r == false then break end

            if size ~= nil then
                results[m] = tonumber(size)
                if options.cache == true then self[m]._size = tonumber(size) end
            end
        end
    end

    if options.close == true then self._cached_close(self) end

    return results
end

function Mailbox._fetch_header(self, messages)
    if not messages or #messages == 0 then return end
    if self._cached_select(self) ~= true then return end

    local results = {}
    for _, m in ipairs(messages) do
        if options.cache == true and
            self[m]._header then
            results[m] = self[m]._header
        else
            self._check_connection(self)
            local r, header = ifcore.fetchheader(self._account._account.session,
                                                 tostring(m))
            self._check_result(self, 'fetchheader', r)
            if r == false then break end

            if header ~= nil then
                results[m] = header
                if options.cache == true then self[m]._header = header end
            end
        end
    end

    if options.close == true then self._cached_close(self) end

    return results
end

function Mailbox._fetch_body(self, messages)
    if not messages or #messages == 0 then return end
    if self._cached_select(self) ~= true then return end

    local results = {}
    for _, m in ipairs(messages) do
        if options.cache == true and
            self[m]._body then
            results[m] = self[m]._body
        else
            self._check_connection(self)
            local r, body = ifcore.fetchbody(self._account._account.session,
                                             tostring(m))
            self._check_result(self, 'fetchbody', r)
            if r == false then break end

            if body ~= nil then
                results[m] = body
                if options.cache == true then self[m]._body = body end
            end
        end
    end

    if options.close == true then self._cached_close(self) end

    return results
end

function Mailbox._fetch_message(self, messages)
    if not messages or #messages == 0 then return end
    if self._cached_select(self) ~= true then return end

    local header = self._fetch_header(self, messages)
    local body = self._fetch_body(self, messages)

    local results = {}
    for _, m in ipairs(messages) do
        if header[m] == nil then
            results[m] = nil
        elseif body[m] == nil then
            results[m] = header[m]
        else
            results[m] = header[m] .. body[m]
        end
    end

    if options.close == true then self._cached_close(self) end

    return results
end


function Mailbox._fetch_fields(self, fields, messages)
    if not messages or #messages == 0 then return end
    if self._cached_select(self) ~= true then return end

    local results = {}
    for _, m in ipairs(messages) do
        results[m] = ''
        for _, f in ipairs(fields) do
            if options.cache == true and
                self[m]._fields[f] then
                results[m] = results[m] .. self[m]._fields[f]
            else
                self._check_connection(self)
                local r, field =
                    ifcore.fetchfields(self._account._account.session,
                                       tostring(m), f)

                self._check_result(self, 'fetchfields', r)
                if r == false then break end

                if field ~= nil then
                    field = string.gsub(field, "\r\n\r\n$", "\n")
                    results[m] = results[m] .. field
                    if options.cache == true then self[m]._fields[f] = field end
                end
            end
        end
        results[m] = string.gsub(results[m], "\n$", "")
    end

    if options.close == true then self._cached_close(self) end

    return results
end

function Mailbox._fetch_structure(self, messages)
    if not messages or #messages == 0 then return end
    if self._cached_select(self) ~= true then return end

    local results = {}
    for _, m in ipairs(messages) do
        if options.cache == true and
            self[m]._structure then
            results[m] = self[m]._structure
        else
            self._check_connection(self)
            local r, structure =
                ifcore.fetchstructure(self._account._account.session,
                                      tostring(m))
            self._check_result(self, 'fetchstructure', r)
            if r == false then break end

            if structure ~= nil then
                local parsed = _parse_structure({ ['s'] = structure,
                                                ['i'] = 1 })
                results[m] = parsed
                if options.cache == true then self[m]._structure = parsed end
            end
        end
    end

    if options.close == true then self._cached_close(self) end

    return results
end

function Mailbox._fetch_parts(self, parts, message)
    if self._cached_select(self) ~= true then return end

    local results = {}
    for _, part in ipairs(parts) do
        results[part] = ''
        if options.cache == true and
            self[message]._parts[part] then
            results[part] = self[message]._parts[part]
        else
            self._check_connection(self)
            local r, bodypart = ifcore.fetchpart(self._account._account.session,
                                                 tostring(message), part)
            self._check_result(self, 'fetchpart', r)
            if r == false then break end

            if bodypart ~= nil then
                results[part] = bodypart
                self[message]._parts[part] = bodypart
            end
        end
    end

    if options.close == true then self._cached_close(self) end

    return results
end


function Mailbox.check_status(self)
    self._check_connection(self)
    local r, exist, recent, unseen, uidnext =
        ifcore.status(self._account._account.session,self._mailbox)
    self._check_result(self, 'status', r)
    if r == false then return false end

    if options.info == true then
        print(string.format("%d messages, %d recent, %d unseen, in %s.",
                            exist, recent, unseen, self._get_details(self)))
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
    if options.info == true and r == true then
        print(string.format("%d messages flagged in %s.", #mesgs,
                            self._get_details(self)))
    end

    return r
end

function Mailbox.remove_flags(self, flags, messages)
    _check_required(flags, 'table')
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)
    local r = self._flag_messages(self, 'remove', flags, mesgs)
    if options.info == true and r == true then
        print(string.format("%d messages flagged in %s.", #mesgs,
                            self._get_details(self)))
    end

    return r
end

function Mailbox.replace_flags(self, flags, messages)
    _check_required(flags, 'table')
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)
    local r = self._flag_messages(self, 'replace', flags, mesgs)
    if options.info == true and r == true then
        print(string.format("%d messages flagged in %s.", #mesgs,
                            self._get_details(self)))
    end

    return r
end


function Mailbox.mark_answered(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)
    local r = self._flag_messages(self, 'add', { '\\Answered' }, mesgs)
    if options.info == true and r == true then
        print(string.format("%d messages marked answered in %s.", #mesgs,
                            self._get_details(self)))
    end

    return r
end


function Mailbox.mark_deleted(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)
    local r = self._flag_messages(self, 'add', { '\\Deleted' }, mesgs)
    if options.info == true and r == true then
        print(string.format("%d messages marked deleted in %s.", #mesgs,
                            self._get_details(self)))
    end

    return r
end

function Mailbox.mark_draft(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)
    local r = self._flag_messages(self, 'add', { '\\Draft' }, mesgs)
    if options.info == true and r == true then
        print(string.format("%d messages marked draft in %s.", #mesgs,
                            self._get_details(self)))
    end

    return r
end

function Mailbox.mark_flagged(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)
    local r = self._flag_messages(self, 'add', { '\\Flagged' }, mesgs)
    if options.info == true and r == true then
        print(string.format("%d messages marked flagged in %s.", #mesgs,
                            self._get_details(self)))
    end

    return r
end

function Mailbox.mark_seen(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)
    local r = self._flag_messages(self, 'add', { '\\Seen' }, mesgs)
    if options.info == true and r == true then
        print(string.format("%d messages marked seen in %s.", #mesgs,
                            self._get_details(self)))
    end

    return r
end

function Mailbox.unmark_answered(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)
    local r = self._flag_messages(self, 'remove', { '\\Answered' }, mesgs)
    if options.info == true and r == true then
        print(string.format("%d messages unmarked answered in %s.", #mesgs,
                            self._get_details(self)))
    end

    return r
end

function Mailbox.unmark_deleted(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)
    local r = self._flag_messages(self, 'remove', { '\\Deleted' }, mesgs)
    if options.info == true and r == true then
        print(string.format("%d messages unmarked deleted in %s.", #mesgs,
                            self._get_details(self)))
    end

    return r
end

function Mailbox.unmark_draft(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)
    local r = self._flag_messages(self, 'remove', { '\\Draft' }, mesgs)
    if options.info == true and r == true then
        print(string.format("%d messages unmarked draft in %s.", #mesgs,
                            self._get_details(self)))
    end

    return r
end

function Mailbox.unmark_flagged(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)
    local r = self._flag_messages(self, 'remove', { '\\Flagged' }, mesgs)
    if options.info == true and r == true then
        print(string.format("%d messages unmarked flagged in %s.", #mesgs,
                            self._get_details(self)))
    end

    return r
end

function Mailbox.unmark_seen(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)
    local r = self._flag_messages(self, 'remove', { '\\Seen' }, mesgs)
    if options.info == true and r == true then
        print(string.format("%d messages unmarked seen in %s.", #mesgs,
                            self._get_details(self)))
    end

    return r
end


function Mailbox.delete_messages(self, messages)
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)
    local r = self._flag_messages(self, 'add', { '\\Deleted' }, mesgs)
    if options.info == true and r == true then
        print(string.format("%d messages deleted in %s.", #mesgs,
                            self._get_details(self)))
    end

    return r
end


function Mailbox.copy_messages(self, dest, messages)
    _check_required(dest, 'table')
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)
    local r = self._copy_messages(self, dest, mesgs)
    if options.info == true and r == true then
        print(string.format("%d messages copied from %s to %s.", #mesgs,
                            self._get_details(self), self._get_details(dest)))
    end

    return r
end


function Mailbox.move_messages(self, dest, messages)
    _check_required(dest, 'table')
    _check_required(messages, 'table')

    local mesgs = _extract_messages(self, messages)
    local rc = self._copy_messages(self, dest, mesgs)
    local rf = false
    if rc == true then
        rf = self._flag_messages(self, 'add', { '\\Deleted' }, mesgs)
    end
    if options.info == true and
        rc == true and rf == true then
        print(string.format("%d messages moved from %s to %s.", #mesgs,
                            self._get_details(self), self._get_details(dest)))
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


function Mailbox.append_message(self, message, flags, date)
    _check_required(message, 'string')
    _check_optional(flags, { 'string', 'table' })
    _check_optional(date, 'string')

    if type(flags) == 'table' then flags = table.concat(flags, ' ') end
    self._check_connection(self)
    r = ifcore.append(self._account._account.session, self._mailbox, message,
                      flags, date)
    self._check_result(self, 'append', r)
    if r == false then return false end

    if options.info == true and r == true then
        print(string.format("Appended message of %d octets to %s.", #message,
                            self._get_details(self)))
    end
    
    return true
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

    if not messages then messages = self._send_query(self) end
    local mesgs = _extract_messages(self, messages)
    local fields = self._fetch_fields(self, { field }, mesgs)
    if #mesgs == 0 or fields == nil then return Set({}) end
    local results = {}
    for m, f in pairs(fields) do
        if regex_search(pattern, (string.gsub(f, "^[^:]*: ?(.*)$", "%1"))) then
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

    if not messages then messages = self._send_query(self) end
    local mesgs = _extract_messages(self, messages)
    local header = self._fetch_header(self, mesgs)
    if #mesgs == 0 or header == nil then return Set({}) end
    local results = {}
    for m, h in pairs(header) do
        if regex_search(pattern, h) then table.insert(results, {self, m}) end
    end

    return Set(results)
end

function Mailbox.match_body(self, pattern, messages)
    _check_required(pattern, 'string')

    if not messages then messages = self._send_query(self) end
    local mesgs = _extract_messages(self, messages)
    local body = self._fetch_body(self, mesgs)
    if #mesgs == 0 or body == nil then return Set({}) end
    local results = {}
    for m, b in pairs(body) do
        if regex_search(pattern, b) then table.insert(results, {self, m}) end
    end

    return Set(results)
end

function Mailbox.match_message(self, pattern, messages)
    _check_required(pattern, 'string')

    if not messages then messages = self._send_query(self) end
    local mesgs = _extract_messages(self, messages)
    local full = self._fetch_message(self, mesgs)
    if #mesgs == 0 or full == nil then return Set({}) end
    local results = {}
    for m, f in pairs(full) do
        if regex_search(pattern, f) then table.insert(results, {self, m}) end
    end

    return Set(results)
end


function Mailbox.enter_idle(self)
    if self._cached_select(self) ~= true then return false end
   
    self._check_connection(self)
    local r = ifcore.idle(self._account._account.session)
    self._check_result(self, 'idle', r)
    if r == false then return false end

    if options.close == true then self._cached_close(self) end

    return true
end

Mailbox.select = _cached_select
Mailbox.close = _cached_close

Mailbox._mt.__index = function () end
Mailbox._mt.__newindex = function () end
