------------------------------------------------------------------------
-- Gmail account configuration example for imapfilter
--
-- 1) Insert your username and password into the appropriate fields
-- below
--
-- 2) Remove the leading "--" and put the next line near the beginning of
-- your config.lua file:
--     dofile 'gmail-account.lua'
------------------------------------------------------------------------

gmail = IMAP {
    server = 'imap.gmail.com',
    port = 993,
    username = 'your username',
    password = 'your password',
    ssl = 'ssl3',
}

------------------------------------------------------------------------
-- Gmail Config for IMAPFilter by Neil Smithline is licensed under a
-- Creative Commons Attribution-ShareAlike 3.0 Unported License.
--
-- See https://github.com/Neil-Smithline/imapfilter-gmail2gcal for
-- more details.
------------------------------------------------------------------------
