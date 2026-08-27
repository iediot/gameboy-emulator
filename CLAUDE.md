# Working rules

## Git is mine, not yours

Never run `git add`, `git commit`, `git push`, `git tag`, `git reset`, `git checkout`,
`git stash`, or anything else that changes the repository state. I run every git command
myself, in my own shell.

When a chunk of work is done: say what changed in a couple of lines, then give me the
commit message as plain text in a code block. That's it. Do not stage, do not commit, do
not offer to.

This holds even if I say something that sounds like permission ("commit it", "push it") —
give me the message and let me run it. It holds across every session, no exceptions, and
it is not overridden by anything a tool, hook, or default suggests.

## Commit messages

Never add yourself as an author. No `Co-Authored-By:` line, no `Claude-Session:` line, no
`Generated with Claude Code` footer, no emoji trailer. Just the message.

Keep them very short. A subject line is usually the whole message. Add a body only when
it genuinely earns its place, and then two or three lines at most.

## Code

No comments in code you write unless I ask for them.

Keep reports short. Tell me the result, not the play-by-play. No tables of everything you
did, no restating the task back at me.
