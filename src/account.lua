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

    for key, value in pairs(Account) do
        if (type(value) == 'function') then
            object[key] = value
        end
    end

    object._mt = {}
    object._mt.__index = object._attach_mailbox
    setmetatable(object, object._mt)

    object._imap = arg

    return object
end


function Account._login_user(self)
    if (self._imap.password == nil) then
        self._imap.password = get_password('Enter password for ' ..
            self._imap.username .. '@' .. self._imap.server .. ': ')
    end

    local r = ifcore.login(self._imap)

    if (r == nil) then
        return true
    elseif (r == true) then
	-- Set mailbox cache to nil so we re-issue a SELECT in
	-- Mailbox._cached_select on reconnecting after disconnect
        self._imap._mailbox = nil
        return true
    elseif (r == false) then
        return false
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

    if (folder == nil) then
        folder = ''
    else
        if (type(options) == 'table' and options.namespace == true) then
            if (folder == '/') then
                folder = ''
            end
            if (folder ~= '') then
                folder = folder .. '/'
            end
        end
    end
    if (mbox == nil) then
        mbox = '%'
    end

    if (self._login_user(self) ~= true) then
        return
    end

    local _, mailboxes, folders = ifcore.list(self._imap, '', folder .. mbox)

    local m = {}
    for s in string.gmatch(mailboxes, '%C+') do
        table.insert(m, s)
    end

    local f = {}
    for s in string.gmatch(folders, '%C+') do
        if s ~= folder and s ~= folder .. '/' then
            table.insert(f, s)
        end
    end

    return m, f
end

function Account.list_subscribed(self, folder, mbox)
    _check_optional(folder, 'string')
    _check_optional(mbox, 'string')

    if (folder == nil) then
        folder = ''
    else
        if (type(options) == 'table' and options.namespace == true) then
            if (folder == '/') then
                folder = ''
            end
            if (folder ~= '') then
                folder = folder .. '/'
            end
        end
    end
    if (mbox == nil) then
        mbox = '*'
    end

    if (self._login_user(self) ~= true) then
        return
    end

    local _, mailboxes, folders = ifcore.lsub(self._imap, '', folder .. mbox)

    local m = {}
    for s in string.gmatch(mailboxes, '%C+') do
        table.insert(m, s)
    end

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

    if (self._login_user(self) ~= true) then
        return
    end

    local r = ifcore.create(self._imap, name)

    if (type(options) == 'table' and options.info == true) then
        print(string.format("Created mailbox %s@%s/%s.",
                            self._imap.username, self._imap.server, name))
    end

    return r
end

function Account.delete_mailbox(self, name)
    _check_required(name, 'string')

    if (self._login_user(self) ~= true) then
        return
    end

    local r = ifcore.delete(self._imap, name)

    if (type(options) == 'table' and options.info == true) then
        print(string.format("Deleted mailbox %s@%s/%s.",
                            self._imap.username, self._imap.server, name))
    end

    return r
end

function Account.rename_mailbox(self, oldname, newname)
    _check_required(oldname, 'string')
    _check_required(newname, 'string')

    if (self._login_user(self) ~= true) then
        return
    end

    local r = ifcore.rename(self._imap, oldname, newname)

    if (type(options) == 'table' and options.info == true) then
        print(string.format("Renamed mailbox %s@%s/%s to %s@%s/%s.",
                            self._imap.username, self._imap.server, oldname,
                            self._imap.username, self._imap.server, newname))
    end

    return r
end

function Account.subscribe_mailbox(self, name)
    _check_required(name, 'string')

    if (self._login_user(self) ~= true) then
        return
    end

    local r = ifcore.subscribe(self._imap, name)

    if (type(options) == 'table' and options.info == true) then
        print(string.format("Subscribed mailbox %s@%s/%s.",
                            self._imap.username, self._imap.server, name))
    end

    return r
end

function Account.unsubscribe_mailbox(self, name)
    _check_required(name, 'string')

    if (self._login_user(self) ~= true) then
        return
    end

    local r = ifcore.unsubscribe(self._imap, name)

    if (type(options) == 'table' and options.info == true) then
        print(string.format("Unsubscribed mailbox %s@%s/%s.",
                            self._imap.username, self._imap.server, name))
    end

    return r
end


Account._mt.__index = function () end
Account._mt.__newindex = function () end
