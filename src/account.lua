-- The Account class represents an IMAP account.

Account = {}
IMAP = Account
imap = Account

Account._mt = {}
setmetatable(Account, Account._mt)


_imap = {}
setmetatable(_imap, { __mode = "v" })


_undefined = 0
function _next_undefined()
    _undefined = _undefined + 1
    return 'undefined' .. _undefined
end


Account._mt.__call = function (self, arg)
    _check_required(arg.server, 'string')
    if not arg.oauth2 then
        _check_required(arg.username, 'string')
    else
        _check_optional(arg.username, 'string')
    end
    _check_optional(arg.password, 'string')
    _check_optional(arg.oauth2, 'string')
    _check_optional(arg.port, 'number')
    _check_optional(arg.ssl, 'string')

    local object = {}

    object._type = 'account'
    object._account = {}
    object._account.server = arg.server
    object._account.username = arg.username or _next_undefined()
    object._account.password = arg.password
    object._account.oauth2 = arg.oauth2
    object._account.port = tostring(arg.port or arg.ssl and 993 or 143)
    object._account.ssl = arg.ssl
    object._account.session = nil
    object._account.selected = nil
    object._string = object._account.username .. '@' .. object._account.server

    for key, value in pairs(Account) do
        if type(value) == 'function' then object[key] = value end
    end

    object._mt = {}
    object._mt.__index = object._attach_mailbox
    object._mt.__gc = object._logout_user
    setmetatable(object, object._mt)

    table.insert(_imap, object)

    object._login_user(object)

    return object
end


function Account._check_connection(self)
    if not self._account.session then
        if not _daemon then
            error('not connected to ' .. self._string, 0)
        else
            return false
        end
    end
    return true
end

function Account._check_result(self, request, result)
    if result == nil then
        self._account.session = nil
        self._account.selected = nil
        if not _daemon then
            error(request .. ' request to ' .. self._string ..  ' failed', 0)
        end
    end
end


function Account._login_user(self)
    if self._account.password == nil and self._account.oauth2 == nil then
            self._account.password = get_password('Enter password for ' ..
                                                  self._string .. ': ')
    end

    if self._account.session then return true end
    local r, s = ifcore.login(self._account.server, self._account.port,
                              self._account.ssl, self._account.username,
                              self._account.password, self._account.oauth2)
    self._check_result(self, 'login', r)
    if r == false then
        error('authentication of ' .. self._string .. ' failed.', 0)
    end
    if not r then return false end

    self._account.session = s
    self._account.selected = nil

    return true
end

function Account._logout_user(self)
    if not self._check_connection(self) then return end
    local r = ifcore.logout(self._account.session)
    self._check_result(self, 'logout', r)
    if r == false then return false end

    self._account.session = nil
    self._account.selected = nil

    return true
end


function Account._attach_mailbox(self, mailbox)
    self[mailbox] = Mailbox(self, mailbox)
    return self[mailbox]
end

function Account._detach_mailbox(self, mailbox)
    self[mailbox] = nil
end


function Account.list_all(self, folder, mbox)
    _check_optional(folder, 'string')
    _check_optional(mbox, 'string')

    if folder == nil then
        folder = ''
    else
        if options.namespace == true then
            if folder == '/' then folder = '' end
            if folder ~= '' then folder = folder .. '/' end
        end
    end
    if mbox == nil then mbox = '%' end

    if not self._check_connection(self) then return end
    local r, mailboxes, folders = ifcore.list(self._account.session, '',
                                              folder .. mbox)
    self._check_result(self, 'list', r)
    if r == false then return false end

    local m = {}
    for s in string.gmatch(mailboxes, '%C+') do table.insert(m, s) end

    local f = {}
    for s in string.gmatch(folders, '%C+') do
        if s ~= folder and s ~= folder .. '/' then table.insert(f, s) end
    end

    return m, f
end

function Account.list_subscribed(self, folder, mbox)
    _check_optional(folder, 'string')
    _check_optional(mbox, 'string')

    if folder == nil then
        folder = ''
    else
        if options.namespace == true then
            if folder == '/' then
                folder = ''
            end
            if folder ~= '' then
                folder = folder .. '/'
            end
        end
    end
    if mbox == nil then mbox = '*' end

    if not self._check_connection(self) then return end
    local r, mailboxes, folders = ifcore.lsub(self._account.session, '',
                                              folder .. mbox)
    self._check_result(self, 'lsub', r)
    if r == false then return false end

    local m = {}
    for s in string.gmatch(mailboxes, '%C+') do table.insert(m, s) end

    local f = {}
    for s in string.gmatch(folders, '%C+') do
        if s ~= folder and s ~= folder .. '/' then
            table.insert(f, s)
        end
    end

    return m, f
end


function Account.create_mailbox(self, name)
    _check_required(name, 'string')

    if not self._check_connection(self) then return end
    local r = ifcore.create(self._account.session, name)
    self._check_result(self, 'create', r)
    if r == false then return false end

    if options.info == true then
        print('Created mailbox ' .. self._string .. '/' .. name .. '.')
    end

    return r
end

function Account.delete_mailbox(self, name)
    _check_required(name, 'string')

    if not self._check_connection(self) then return end
    local r = ifcore.delete(self._account.session, name)
    self._check_result(self, 'delete', r)
    if r == false then return false end

    if options.info == true then
        print('Deleted mailbox ' .. self._string .. '/' .. name .. '.')
    end

    return r
end

function Account.rename_mailbox(self, oldname, newname)
    _check_required(oldname, 'string')
    _check_required(newname, 'string')

    if not self._check_connection(self) then return end
    local r = ifcore.rename(self._account.session, oldname, newname)
    self._check_result(self, 'rename', r)
    if r == false then return false end

    if options.info == true then
        print('Renamed mailbox ' .. self._string .. '/' .. oldname .. ' to ' ..
              self._string .. '/' .. newname .. '.')
    end

    return r
end

function Account.subscribe_mailbox(self, name)
    _check_required(name, 'string')

    if not self._check_connection(self) then return end
    local r = ifcore.subscribe(self._account.session, name)
    self._check_result(self, 'subscribe', r)
    if r == false then return false end

    if options.info == true then
        print('Subscribed mailbox ' .. self._string .. '/' .. name .. '.')
    end

    return r
end

function Account.unsubscribe_mailbox(self, name)
    _check_required(name, 'string')

    if not self._check_connection(self) then return end
    local r = ifcore.unsubscribe(self._account.session, name)
    self._check_result(self, 'unsubscribe', r)
    if r == false then return false end

    if options.info == true then
        print('Unsubscribed mailbox ' .. self._string .. '/' .. name .. '.')
    end

    return r
end

Account.login = Account._login_user
Account.logout = Account._logout_user

Account._mt.__index = function () end
Account._mt.__newindex = function () end
