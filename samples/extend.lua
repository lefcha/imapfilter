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
-- file.
--
-- The file is encrypted using the openssl(1) command line tool.  For
-- example the "passwords.txt" file:
--
--   secret1
--   secret2
--
-- ... is encrypted and saved to a file named "passwords.enc" with the
-- command:
--
--   $ openssl bf -in passwords.txt -out passwords.enc
--
-- The auxiliary function pipe_from() is supplied for conveniency.  The
-- user is prompted to enter the decryption password, the file is
-- decrypted and the account passwords are set accordingly:

status, output = pipe_from('openssl bf -d -in ~/passwords.enc')

_, _, password1, password2 = string.find(output, '([%w%p]+)\n([%w%p]+)')

account1 = IMAP {
    server = 'imap1.mail.server',
    username = 'user1',
    password = password1
}

account2 = IMAP {
    server = 'imap2.mail.server',
    username = 'user2',
    password = password2
}


-- An alternative way to authenticate to a server is by using a OAuth2 string,
-- if the server supports the XOAUTH2 authentication mechanism.
--
-- In order to generate an OAuth2 string the oauth2.py script and library can
-- be used, and instructions on how to use it and where to download it are
-- available at:
--
--   https://github.com/google/gmail-oauth2-tools/wiki/OAuth2DotPyRunThrough
--
-- The generated OAuth2 string is then supplied to imapfilter in order to
-- authenticate to the IMAP server using it instead of a login
-- username/password pair.
--
-- Here we assume that imapfilter has the user, the cliend id, the client
-- secret and the refresh token, and uses them to generate a new access token
-- (access tokens expire after one hour), and then from the new access token to
-- generate the OAuth2 string that is used with the IMAP server:
user = 'xoauth@gmail.com'
clientid = '364545978226.apps.googleusercontent.com'
clientsecret = 'zNrNsBzOOnQy8_O-8LkofeTR'
refreshtoken = '1/q4SaB2JMQB9I-an6F1rxJE9OkOMtfjaz1bPm1tfDpQM'

status, output = pipe_from('oauth2.py --client_id=' .. clientid ..
                 ' --client_secret=' .. clientsecret ..
                 ' --refresh_token=' .. refreshtoken)
_, _, accesstoken = string.find(output, 'Access Token: ([%w%p]+)\n')

status, output = pipe_from('oauth2.py --generate_oauth2_string' ..
                           ' --access_token=' .. accesstoken ..
                           ' --user=' .. user)
_, _, oauth2string = string.find(output, 'OAuth2 argument:\n([%w%p]+)\n')

account3 = IMAP {
    server = 'imap.gmail.com',
    ssl = 'tls1.2',
    username = user,
    oauth2 = oauth2string
}
