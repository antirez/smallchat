# Smallchat

TLDR: This is just a programming example for a few friends of mine. Long story follows.

Yesterday I was talking with a few friends of mine, front-end developers mostly, that are a bit far from system programming. We were remembering the old times of IRC. And inevitably I said: to write a very simple IRC server is an experience everybody should do. There are very interesting parts in a program like that. A single process doing multiplexing, taking the client state, that can be done in different ways, and so forth.

But then the discussion evolved and I thought, I'll show you a very minimal example in C. But what is the smallest chat server you can write? For starters to be truly minimal we should not require any proper client. Even if not very well, it should work with `telnet` or `nc` (netcat). The server main operation is just to receive some chat line and send it to all the other clients, in what is sometimes called a fan-out operation. But yet, this would require a proper readline() function, then buffering, and so forth. We want it simpler: let's cheat using the kernel buffers, and pretending we every time receive a full-formed line from the client (an assumption that is in the practice often true, so things kinda work).

Well, with this tricks we can implement a chat that even has the ability to
let the user set their nick in just 200 lines of code (removing spaces
and comments, of course). Since I wrote this little program as an example for
my friends, I decided to also push it here on Github.
