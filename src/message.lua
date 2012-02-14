-- The Message class that represents messages inside a mailbox.

Message = {}

Message._mt = {}
setmetatable(Message, Message._mt)


Message._mt.__call = function (self, account, mailbox, uid)
    local object = {}

    object._type = 'message'
    object._account = account
    object._mailbox = mailbox
    object._uid = uid

    object._structure = nil
    object._header = nil
    object._body = nil
    object._fields = {}
    object._parts = {}
    object._size = nil
    object._date = nil

    for key, value in pairs(Message) do
        if type(value) == 'function' then object[key] = value end
    end

    object._mt = {}
    setmetatable(object, object._mt)

    return object
end


function Message.fetch_structure(self)
    local r = self._mailbox._fetch_structure(self._mailbox, { self._uid })
    return r[self._uid]
end

function Message.fetch_header(self)
    local r = self._mailbox._fetch_header(self._mailbox, { self._uid })
    return r[self._uid]
end

function Message.fetch_body(self)
    local r = self._mailbox._fetch_body(self._mailbox, { self._uid })
    return r[self._uid]
end

function Message.fetch_message(self)
    local r = self._mailbox._fetch_message(self._mailbox, { self._uid })
    return r[self._uid]
end

function Message.fetch_field(self, field)
    local r = self._mailbox._fetch_fields(self._mailbox, { field },
        { self._uid })
    return r[self._uid]
end

function Message.fetch_fields(self, fields)
    local r = self._mailbox._fetch_fields(self._mailbox, fields, { self._uid })
    return r[self._uid]
end

function Message.fetch_part(self, part)
    local r = self._mailbox._fetch_parts(self._mailbox, { part }, self._uid)
    return r[part]
end

function Message.fetch_size(self)
    local r = self._mailbox._fetch_size(self._mailbox, { self._uid })
    return r[self._uid]
end

function Message.fetch_date(self)
    local r = self._mailbox._fetch_date(self._mailbox, { self._uid })
    return r[self._uid]
end

function Message.fetch_flags(self)
    local r = self._mailbox._fetch_flags(self._mailbox, { self._uid })
    return r[self._uid]
end


Message._mt.__index = function () end
Message._mt.__newindex = function () end
