---------------
--  Options  --
---------------

options.timeout = 120
options.subscribe = true


----------------
--  Accounts  --
----------------

-- Connects to "imap1.mail.server", as user "user1" with "secret1" as
-- password.
account1 = IMAP {
    server = 'imap1.mail.server',
    username = 'user1',
    password = 'secret1',
}

-- Another account which connects to the mail server using the SSLv3
-- protocol.
account2 = IMAP {
    server = 'imap2.mail.server',
    username = 'user2',
    password = 'secret2',
    ssl = 'ssl23',
}

-- Get a list of the available mailboxes and folders
mailboxes, folders = account1:list_all()

-- Get a list of the subscribed mailboxes and folders
mailboxes, folders = account1:list_subscribed()

-- Create a mailbox
account1:create_mailbox('Friends')

-- Subscribe a mailbox
account1:subscribe_mailbox('Friends')


-----------------
--  Mailboxes  --
-----------------

-- Get the status of a mailbox
account1.INBOX:check_status()

-- Get all the messages in the mailbox.
results = account1.INBOX:select_all()

-- Get newly arrived, unread messages
results = account1.INBOX:is_new()

-- Get unseen messages with the specified "From" header.
results = account1.INBOX:is_unseen() *
          account1.INBOX:contain_from('weekly-news@news.letter')

-- Copy messages between mailboxes at the same account.
results:copy_messages(account1.news)

-- Get messages with the specified "From" header but without the
-- specified "Subject" header.
results = account1.INBOX:contain_from('announce@my.unix.os') -
          account1.INBOX:contain_subject('security advisory')

-- Copy messages between mailboxes at a different account.
results:copy_messages(account2.security)

-- Get messages with any of the specified headers.
results = account1.INBOX:contain_from('marketing@company.junk') +
          account1.INBOX:contain_from('advertising@annoying.promotion') +
          account1.INBOX:contain_subject('new great products')

-- Delete messages.
results:delete_messages()

-- Get messages with the specified "Sender" header, which are older than
-- 30 days.
results = account1.INBOX:contain_field('sender', 'owner@announce-list') *
          account1.INBOX:is_older(30)

-- Move messages to the "announce" mailbox inside the "lists" folder.
results:move_messages(account1['lists/announce'])

-- Get messages, in the "devel" mailbox inside the "lists" folder, with the
-- specified "Subject" header and a size less than 50000 octets (bytes).
results = account1['lists/devel']:contain_subject('[patch]') *
          account1['lists/devel']:is_smaller(50000)

-- Move messages to the "patch" mailbox.
results:move_messages(account2.patch)

-- Get recent, unseen messages, that have either one of the specified
-- "From" headers, but do not have the specified pattern in the body of
-- the message.
results = ( account1.INBOX:is_recent() *
            account1.INBOX:is_unseen() *
            ( account1.INBOX:contain_from('tux@penguin.land') +
              account1.INBOX:contain_from('beastie@daemon.land') ) ) -
          account1.INBOX:match_body('.*all.work.and.no.play.*')

-- Mark messages as important.
results:mark_flagged()

-- Get all messages in two mailboxes residing in the same server.
results = account1.news:select_all() +
          account1.security:select_all()

-- Mark messages as seen.
results:mark_seen()

-- Get recent messages in two mailboxes residing in different servers.
results = account1.INBOX:is_recent() +
          account2.INBOX:is_recent()

-- Flag messages as seen and important.
results:add_flags({ '\\Seen', '\\Flagged' })

-- Get unseen messages.
results = account1.INBOX:is_unseen()

-- From the messages that were unseen, match only those with the specified
-- regular expression in the header.
newresults = results:match_header('^.+MailScanner.*Check: [Ss]pam$')

-- Delete those messages.
newresults:delete_messages()
