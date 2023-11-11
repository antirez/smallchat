# Smallchat

TLDR: This is just a programming example for a few friends of mine. It somehow turned into a set of programming videos, continuing one project I started some time ago: Writing System Software videos series.

1. [First episode](https://www.youtube.com/watch?v=eT02gzeLmF0), how the basic server works.
2. [Second episode](https://youtu.be/yogoUJ2zVYY), writing a simple client with raw terminal handling.

Likely more will follow, stay tuned.

**IMPORTANT: a warning about PRs**: please note that most pull requests adding features will be refused, because the point of this repository is to improve it step by step in the next videos. We will do refactoring during live coding sessions (or explaining how the refactoring was needed in the video), introducing more libraries to improve the program inner working (linenoise, rax, and so forth). So if you want to improve the program as an exercise, go ahead! It's a great idea. But I will not merge new features here since the point of the program is to evolve it step by step during the videos.

## And now, the full story:

Yesterday I was talking with a few friends of mine, front-end developers mostly, who are a bit far from system programming. We were remembering the old times of IRC. And inevitably I said: that writing a very simple IRC server is an experience everybody should do (I showed them my implementation written in TCL; I was quite shocked that I wrote it 18 years ago: time passes fast). There are very interesting parts in such a program. A single process doing multiplexing, taking the client state and trying to access such state fast once a client has new data, and so forth.

But then the discussion evolved and I thought, I'll show you a very minimal example in C. What is the smallest chat server you can write? For starters to be truly minimal we should not require any proper client. Even if not very well, it should work with `telnet` or `nc` (netcat). The server's main operation is just to receive some chat line and send it to all the other clients, in what is sometimes called a fan-out operation. However, this would require a proper `readline()` function, then buffering, and so forth. We want it simpler: let's cheat using the kernel buffers, and pretending we every time receive a full-formed line from the client (an assumption that is in practice often true, so things kinda work).

Well, with these tricks we can implement a chat that even has the ability to let the user set their nick in just 200 lines of code (removing spaces and comments, of course). Since I wrote this little program as an example for my friends, I decided to also push it here on GitHub.

## How to build on Windows

One way is to use a virtual machine, of course, if you find it troublesome, you can use an emulated terminal.Here I introduce a tool that I use in practice.

First, you need to download Cygwin.

Remember, you need to select additional git，ssh，vim，gcc/make, telnet/netcat or other packages you need during installation, which will be used for compilation and operation later.

Then, fork the code, create ssh key in Cygwin and add it to your github. After that, you can clone it to Cygwin and create a branch.

Next you can choose to use 'make' or 'gcc smallchat.c -o smallchat` to compile the code.

Finally, open a window first to run ./smallchat under the folder where the executable file is located.Then open two other windows, run nc localhost 7711, and use /nick yourname to modify the nickname respectively, and now you can speak freely in the chat room.

## Future work

In the next few days, I'll continue to modify this program in order to evolve it. Different evolution steps will be tagged according to the YouTube episode of my series on *Writing System Software* covering such changes. This is my plan (may change, but more or less this is what I want to cover):

* Implementing buffering for reading and writing.
* Avoiding the linear array, using a dictionary data structure to hold the client state.
* Writing a proper client: line editing able to handle asynchronous events.
* Implementing channels.
* Switching from select(2) to more advanced APIs.
* Simple symmetric encryption for the chat.

Different changes will be covered by one or more YouTube videos. The full commit history will be preserved in this repository.
