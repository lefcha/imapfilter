-- Miscellaneous auxiliary functions.

function form_date(days)
    _check_required(days, 'number')
    return os.date('%d-%b-%Y', os.time() - days * 60 * 60 * 24)
end


function get_password(prompt)
    _check_optional(prompt, 'string')

    if prompt ~= nil then
        io.write(prompt)
    else
        io.write('Enter password: ')
    end
    io.flush()
    ifsys.noecho()
    local p = io.read()
    ifsys.echo()

    return p
end


function pipe_to(command, data)
    _check_required(command, 'string')
    _check_required(data, 'string')

    f = ifsys.popen(command, 'w')
    ifsys.write(f, data)

    return ifsys.pclose(f)
end

function pipe_from(command)
    _check_required(command, 'string')

    f = ifsys.popen(command, 'r')
    local string = ''
    while true do
        s = ifsys.read(f)
        if s ~= nil then
            string = string .. s
        else
            break
        end
    end

    return ifsys.pclose(f), string
end


function become_daemon(interval, commands, nochdir, noclose)
    _check_required(interval, 'number')
    _check_required(commands, 'function')
    _check_optional(nochdir, 'boolean')
    _check_optional(noclose, 'boolean')

    if nochdir == nil then nochdir = false end
    if noclose == nil then noclose = false end
    ifsys.daemon(nochdir, noclose)
    _daemon = true
    repeat
        for _, account in pairs(_imap) do
            if not account._account.session then
                account:_login_user(account)
            end
        end
        commands()
        collectgarbage()
    until ifsys.sleep(interval) ~= 0
end

sleep = ifsys.sleep
