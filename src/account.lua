-- The Account class represents an IMAP account.

Account = {}
IMAP = Account
imap = Account

Account._mt = {}
setmetatable(Account, Account._mt)


Account._mt.__call = function (self, arg)
    _check_required(arg.server, 'string')
    _check_required(arg.username, 'string')
    _check_optional(arg.password, 'string')
    _check_optional(arg.port, 'number')
    _check_optional(arg.ssl, 'string')

    local object = {}

    object._type = 'account'
    object._server = arg.server
    object._username = arg.username
    object._password = arg.password
    object._port = tostring(arg.port or arg.ssl and 993 or 143)
    object._ssl = arg.ssl or ''
    object._session = nil
    object._selected = nil

    for key, value in pairs(Account) do
        if type(value) == 'function' then object[key] = value end
    end

    object._mt = {}
    object._mt.__index = object._attach_mailbox
    object._mt.__gc = object._logout_user
    setmetatable(object, object._mt)

    object:_login_user()

    return object
end


function Account._login_user(self)
    if self._password == nil then
        self._password = get_password('Enter password for ' ..
            self._username .. '@' .. self._server .. ': ')
    end

    local r, s = ifcore.login(self._server, self._port, self._ssl,
                              self._username, self._password)

    if r == true then
        self._session = s
        self._selected = nil
        return true
    elseif r == false then
        return true
    elseif r == nil then
        error("login request failed", 0)
    end
end

function Account._logout_user(self)
    local r = ifcore.logout(self._session)
    if r == true then
        self._session = nil
        self._selected = nil
    elseif r == nil then
        error("logout request failed", 0)
    end
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

    local r, mailboxes, folders = ifcore.list(self._session, '', folder .. mbox)

    if r == false then
        return false
    elseif r == nil then
        error("list request failed", 0)
    end

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

    local r, mailboxes, folders = ifcore.lsub(self._session, '', folder .. mbox)

    if r == false then
        return false
    elseif r == nil then
        error("lsub request failed", 0)
    end

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

    local r = ifcore.create(self._session, name)

    if r == nil then error("create request failed", 0) end

    if options.info == true then
        print(string.format("Created mailbox %s@%s/%s.",
                            self._username, self._server, name))
    end

    return r
end

function Account.delete_mailbox(self, name)
    _check_required(name, 'string')

    local r = ifcore.delete(self._session, name)

    if r == nil then error("delete request failed", 0) end

    if options.info == true then
        print(string.format("Deleted mailbox %s@%s/%s.",
                            self._username, self._server, name))
    end

    return r
end

function Account.rename_mailbox(self, oldname, newname)
    _check_required(oldname, 'string')
    _check_required(newname, 'string')

    local r = ifcore.rename(self._session, oldname, newname)

    if r == nil then error("rename request failed", 0) end

    if options.info == true then
        print(string.format("Renamed mailbox %s@%s/%s to %s@%s/%s.",
                            self._username, self._server, oldname,
                            self._username, self._server, newname))
    end

    return r
end

function Account.subscribe_mailbox(self, name)
    _check_required(name, 'string')

    local r = ifcore.subscribe(self._session, name)

    if r == nil then error("subscribe request failed", 0) end

    if options.info == true then
        print(string.format("Subscribed mailbox %s@%s/%s.",
                            self._username, self._server, name))
    end

    return r
end

function Account.unsubscribe_mailbox(self, name)
    _check_required(name, 'string')

    local r = ifcore.unsubscribe(self._session, name)

    if r == nil then error("unsubscribe request failed", 0) end

    if options.info == true then
        print(string.format("Unsubscribed mailbox %s@%s/%s.",
                            self._username, self._server, name))
    end

    return r
end


Account._mt.__index = function () end
Account._mt.__newindex = function () end
