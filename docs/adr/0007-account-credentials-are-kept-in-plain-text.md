# Account credentials are kept in plain text

The accounts a player saves are kept as an account name and a password in a JSON
file of BH's own, `BH_accounts.json`, beside the settings. The password is
written as typed. Nothing about the file is encrypted, obfuscated or bound to the
machine it was written on.

This is written down because it is the opposite of what a reader expects of a
credential store, and because the alternative was on the table rather than
overlooked.

## Considered options

Encrypting each password with DPAPI, which is `CryptProtectData` without the
local machine flag, was rejected. It binds the ciphertext to the Windows user
account, so a file that leaves the machine says nothing, and it costs the player
nothing to type. What it does not defeat is anything running as the player,
which on a machine running an injected DLL is not a small exclusion. It also
makes each password a binary blob in a file whose whole reason for being JSON is
that a person can read it when something has gone wrong, and it makes the file
unrecoverable rather than merely exposed when Windows is reinstalled. Taking a
real cost against a threat it half answers was not a trade worth making here.

Obfuscating the passwords, by XOR or by base64, was rejected outright. It
defeats exactly what plain text defeats, which is nothing, while telling a
reader of the file that someone thought about the problem. A store that lies
about what it protects is worse than one that does not pretend.

Asking for a master password once per launch was rejected because the feature
exists for a player opening several clients at a time. Anything typed per launch
is multiplied by the number of instances, which is the cost the feature was
meant to remove.

## Consequences

The threat this does not answer is the one to say out loud: any process running
as the player, and anyone who reads the file, has every account. The threats it
does answer are the ones players actually lose accounts to, which are a game
folder copied to another machine, a settings directory inside a synced folder,
and a config pasted into a chat window while asking for help. None of those are
answered by encryption either, once the file has left. They are answered by the
file being one file, named for what it holds, that nothing else asks a player to
share.

`Default Password` in the lobby settings has been written in plain text in
`BH_settings.cfg` since long before this, so the file is a new place for
credentials rather than a new kind of exposure.

Because the file is BH's own and no player is asked to hand-edit it, its shape
can change without breaking anyone's work. That is what leaves the door open: a
later version can encrypt what it writes and read both, and the migration is
BH's to perform rather than the player's.
