--
-- This file contains examples on how IMAPFilter can be extended using
-- the Lua programming language.
--


-- IMAPFilter can be detached from the controlling terminal and run in
-- the background as a system daemon.
--
-- The auxiliary function become_daemon() is supplied for conveniency.
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
    results = myaccount.mymailbox:is_unread()
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
    results = myaccount.mymailbox:is_unread()
    results:move_messages(myaccount.myothermailbox)
end


-- IMAPFilter can take advantage of all those filtering utilities that
-- are available and use a wide range of heuristic tests, text analysis,
-- internet-based realtime blacklists, advanced learning algorithms,
-- etc. to classify mail.  IMAPFilter can pipe a message to a program
-- and act on the message based on the program's exit status.
--
-- The auxiliary function pipe_to() is supplied for conveniency.  For
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
account1 = IMAP {
    server = 'imap1.mail.server',
    username = 'user1',
    password = password
}
