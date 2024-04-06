--
-- This file contains examples on how IMAPFilter can be extended using
-- the Lua programming language.
--


-- IMAPFilter can be detached from the controlling terminal and run in
-- the background as a system daemon.
--
-- The auxiliary function become_daemon() is supplied for convenience.
-- The following example puts imapfilter in the background and runs
-- endlessly, executing the commands in the forever() function and
-- sleeping for 600 seconds between intervals:

function forever()
    results = myaccount.mymailbox:is_old()
    results:move_messages(myaccount.myothermailbox)
end

become_daemon(600, forever)


-- The previous example uses polling in order to search specific messages and
-- process them.  Another more efficient alternative is using the IMAP IDLE
-- extension.  This is implemented by the enter_idle() method, which waits for
-- a notification by the server when new messages arrive in the monitored
-- mailbox.

while true do
    myaccount.mymailbox:enter_idle()
    results = myaccount.mymailbox:is_unseen()
    results:move_messages(myaccount.myothermailbox)
end


-- The previous example can be further improved to consider whether new
-- messages arrived while filtering took place, and also somewhat work on
-- servers that have no IDLE support.

function custom_idle(mbox)
    if #mbox:is_unseen() == 0 then
        if not mbox:enter_idle() then
            sleep(300)
        end
    end
end

while true do
    custom_idle(myaccount.mymailbox)
    results = myaccount.mymailbox:is_unseen()
    results:move_messages(myaccount.myothermailbox)
end


-- IMAPFilter can take advantage of all those filtering utilities that
-- are available and use a wide range of heuristic tests, text analysis,
-- internet-based real-time blacklists, advanced learning algorithms,
-- etc. to classify mail.  IMAPFilter can pipe a message to a program
-- and act on the message based on the program's exit status.
--
-- The auxiliary function pipe_to() is supplied for convenience.  For
-- example if there was a utility named "bayesian-spam-filter", which
-- returned 1 when it considered the message "spam" and 0 otherwise:

all = myaccount.mymailbox:select_all()

results = Set {}
for _, mesg in ipairs(all) do
    mbox, uid = table.unpack(mesg)
    text = mbox[uid]:fetch_message()
    if (pipe_to('bayesian-spam-filter', text) == 1) then
        table.insert(results, mesg)
    end
end

results:delete_messages()


-- One might want to run the bayesian filter only in those parts (attachments)
-- of the message that are of type text/plain and smaller than 1024 bytes.
-- This is possible using the fetch_structure() and fetch_part() functions:

all = myaccount.mymailbox:select_all()

results = Set {}
for _, mesg in ipairs(all) do
    mbox, uid = table.unpack(mesg)
    structure = mbox[uid]:fetch_structure()
    for partid, partinf in pairs(structure) do
        if partinf.type:lower() == 'text/plain' and partinf.size < 1024 then
            part = mbox[uid]:fetch_part(partid)
            if (pipe_to('bayesian-spam-filter', part) == 1) then
                table.insert(results, mesg)
                break
            end
        end
    end
end

results:delete_messages()


-- Messages can be appended to a mailbox.  One can fetch a message from a
-- mailbox, optionally process it, and then upload it to the same or different
-- mailbox, at the same or different mail servers.  In the following example a
-- header field is added to all messages, and the processed messages are then
-- appended to a different mailbox.

all = myaccount.mymailbox:select_all()

for _, mesg in ipairs(all) do
    mbox, uid = table.unpack(mesg)
    header = mbox[uid]:fetch_header()
    body = mbox[uid]:fetch_body()
    message = header:gsub('[\r\n]+$', '\r\n') ..
              'My-Header: My-Content\r\n' .. '\r\n' .. body
    myaccount.myothermaibox:append_message(message)
end


-- Passwords could be extracted during execution time from an encrypted
-- password vault. Here's an example using pass.

status, password = pipe_from('pass Email/imap1.mail.server')
password = password:gsub('[\r\n]', '')
account1 = IMAP {
    server = 'imap1.mail.server',
    username = 'user1',
    password = password
}

-- Normally when there's a problem, such as network failure to receive data
-- from or send data to the IMAP server, an error is raised, IMAPFilter stops
-- execution of the user's config, and quits.
--
-- In previous versions, IMAPFilter had an option to enable automatic recovery,
-- in which case it tried to reconnect, repeat the last IMAP command, and then
-- continue execution of the user's config.
--
-- Recently, this functionality was replaced by a new, more advanced, robust,
-- and flexible mechanism, where recovery can be fully controlled by the user,
-- using the auxiliary recover() function.
--
-- This example shows that set of commands can be grouped together in a
-- function, and if there's an error, the function will be re-executed from
-- start, once a connection has been re-established with the server:

function commands()
    results = myaccount.mymailbox:is_old()
    results:move_messages(myaccount.myothermailbox)
end

recover(commands)

-- When there are multiple accounts, where a series of actions are taking place
-- repeatedly, recovery can be controlled in such a way, that the rest of the
-- accounts commands are still processed, and the failure of a single account,
-- doesn't block the rest:

function commands1()
    results = myaccount.mymailbox:is_old()
    results:move_messages(myaccount.myothermailbox)
end

function commands2()
    results2 = myaccount2.mymailbox2:is_seen()
    results2:delete_messages()
end

while true do
    recover(commands1, 4)
    recover(commands2, 2)
end

-- It is also possible to handle the error captured during failure, or even
-- further process the returned values of the function executed:

function commands1()
    results = myaccount.mymailbox:is_old()
    results:move_messages(myaccount.myothermailbox)
end

function commands2()
    return myaccount2.mymailbox2:is_seen()
end

success, errormsg = recover(commands1, 5)
if not success then print(errormsg) end

success2, results2 = recover(commands2)
if success2 then results2:delete_messages() end
