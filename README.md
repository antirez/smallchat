# Smallchat

TLDR: This is just a programming example for a few friends of mine. I uploaded a video on my YouTube channel [zooming into the code](https://www.youtube.com/watch?v=eT02gzeLmF0), to see what can be learned from a so simple and broken (on purpose) example. More videos and improvements will follow, see the end of this README file.

And now, the full story:

Yesterday I was talking with a few friends of mine, front-end developers mostly, who are a bit far from system programming. We were remembering the old times of IRC. And inevitably I said: that writing a very simple IRC server is an experience everybody should do (I showed them my implementation written in TCL; I was quite shocked that I wrote it 18 years ago: time passes fast). There are very interesting parts in such a program. A single process doing multiplexing, taking the client state and trying to access such state fast once a client has new data, and so forth.

But then the discussion evolved and I thought, I'll show you a very minimal example in C. What is the smallest chat server you can write? For starters to be truly minimal we should not require any proper client. Even if not very well, it should work with `telnet` or `nc` (netcat). The server's main operation is just to receive some chat line and send it to all the other clients, in what is sometimes called a fan-out operation. However, this would require a proper `readline()` function, then buffering, and so forth. We want it simpler: let's cheat using the kernel buffers, and pretending we every time receive a full-formed line from the client (an assumption that is in practice often true, so things kinda work).

Well, with these tricks we can implement a chat that even has the ability to let the user set their nick in just 200 lines of code (removing spaces and comments, of course). Since I wrote this little program as an example for my friends, I decided to also push it here on GitHub.

## Future work

In the next few days, I'll continue to modify this program in order to evolve it. Different evolution steps will be tagged according to the YouTube episode of my series on *Writing System Software* covering such changes. This is my plan (may change, but more or less this is what I want to cover):

* Implementing buffering for reading and writing.
* Avoiding the linear array, using a dictionary data structure to hold the client state.
* Writing a proper client: line editing able to handle asynchronous events.
* Implementing channels.
* Switching from select(2) to more advanced APIs.
* Simple symmetric encryption for the chat.

Different changes will be covered by one or more YouTube videos. The full commit history will be preserved in this repository.
