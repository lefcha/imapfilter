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
    object._account = {}
    object._account.server = arg.server
    object._account.username = arg.username
    object._account.password = arg.password
    object._account.port = tostring(arg.port or arg.ssl and 993 or 143)
    object._account.ssl = arg.ssl or ''
    object._account.session = nil
    object._account.selected = nil

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
    if self._account.password == nil then
        self._account.password = get_password('Enter password for ' ..
            self._account.username .. '@' .. self._account.server .. ': ')
    end

    if self._account.session then return true end
    local r, s = ifcore.login(self._account.server, self._account.port,
                              self._account.ssl, self._account.username,
                              self._account.password)

    if r == true then
        self._account.session = s
        self._account.selected = nil
        return true
    elseif r == false then
        return true
    elseif r == nil then
        error("login request failed", 0)
    end
end

function Account._logout_user(self)
    if not self._account.session then error("not connected", 0) end
    local r = ifcore.logout(self._account.session)
    if r == true then
        self._account.session = nil
        self._account.selected = nil
    elseif r == nil then
        self._account.session = nil
        self._account.selected = nil
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

    if not self._account.session then error("not connected", 0) end
    local r, mailboxes, folders = ifcore.list(self._account.session, '',
                                              folder .. mbox)

    if r == false then
        return false
    elseif r == nil then
        self._account.session = nil
        self._account.selected = nil
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

    if not self._account.session then error("not connected", 0) end
    local r, mailboxes, folders = ifcore.lsub(self._account.session, '',
                                              folder .. mbox)

    if r == false then
        return false
    elseif r == nil then
        self._account.session = nil
        self._account.selected = nil
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

    if not self._account.session then error("not connected", 0) end
    local r = ifcore.create(self._account.session, name)

    if r == nil then
        self._account.session = nil
        self._account.selected = nil
        error("create request failed", 0)
    end

    if options.info == true then
        print(string.format("Created mailbox %s@%s/%s.",
                            self._account.username, self._account.server, name))
    end

    return r
end

function Account.delete_mailbox(self, name)
    _check_required(name, 'string')

    if not self._account.session then error("not connected", 0) end
    local r = ifcore.delete(self._account.session, name)

    if r == nil then
        self._account.session = nil
        self._account.selected = nil
        error("delete request failed", 0)
    end

    if options.info == true then
        print(string.format("Deleted mailbox %s@%s/%s.",
                            self._account.username, self._account.server, name))
    end

    return r
end

function Account.rename_mailbox(self, oldname, newname)
    _check_required(oldname, 'string')
    _check_required(newname, 'string')

    if not self._account.session then error("not connected", 0) end
    local r = ifcore.rename(self._account.session, oldname, newname)

    if r == nil then
        self._account.session = nil
        self._account.selected = nil
        error("rename request failed", 0)
    end

    if options.info == true then
        print(string.format("Renamed mailbox %s@%s/%s to %s@%s/%s.",
                            self._account.username, self._account.server,
                            oldname, self._account.username,
                            self._account.server, newname))
    end

    return r
end

function Account.subscribe_mailbox(self, name)
    _check_required(name, 'string')

    if not self._account.session then error("not connected", 0) end
    local r = ifcore.subscribe(self._account.session, name)

    if r == nil then
        self._account.session = nil
        self._account.selected = nil
        error("subscribe request failed", 0)
    end

    if options.info == true then
        print(string.format("Subscribed mailbox %s@%s/%s.",
                            self._account.username, self._account.server, name))
    end

    return r
end

function Account.unsubscribe_mailbox(self, name)
    _check_required(name, 'string')

    if not self._account.session then error("not connected", 0) end
    local r = ifcore.unsubscribe(self._account.session, name)

    if r == nil then
        self._account.session = nil
        self._account.selected = nil
        error("unsubscribe request failed", 0)
    end

    if options.info == true then
        print(string.format("Unsubscribed mailbox %s@%s/%s.",
                            self._account.username, self._account.server, name))
    end

    return r
end


Account._mt.__index = function () end
Account._mt.__newindex = function () end
