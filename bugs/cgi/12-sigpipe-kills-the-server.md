# 12 — `SIGPIPE` is not ignored, so a pipe write can kill the whole server

**Where:** `main.cpp` → `main()`

## Status: hardening, not an observed crash

Be clear about what this one is. I could **not** get the server to die from
`SIGPIPE`, before or after the CGI rework, and the write-up says so rather than
inventing a failing test. The fix is one line and it stays, for the reasons
below.

## Why it cannot fire today

* **Before the CGI rework** the body written to the child was always empty
  ([08](08-cgi-body-lost-and-deadlock.md)), and `write(fd, "", 0)` never raises
  `SIGPIPE`.
* **After the rework** the two CGI pipes are registered in a fixed order —
  `stdout` first, then `stdin` ([06](06-cgi-connection-dropped.md)). When a
  script exits early, both fds become ready in the same round, the `stdout` EOF
  is handled first, and `releaseCgi()` closes the input side before any further
  write is attempted.

Reproduction attempt, with the `signal(SIGPIPE, SIG_IGN)` line commented out and
a script that reads 100 bytes of a 5 MB body and exits:

```
run1 http=200 alive=1
run2 http=200 alive=1
run3 http=200 alive=1
```

The server survived every time.

## Why it is still a bug

The protection is accidental. It rests on the order two entries happen to sit
in the interest list and on both events landing in the same batch. Any of these
breaks it, none of them is unreasonable:

* registering the input pipe before the output pipe,
* the child dying between a partial write and the next wait,
* a future `write()` to a socket that forgets `MSG_NOSIGNAL`,
* a change of multiplexer with a different ready order — which
  [31](../epoll/31-epoll-event-loop.md) is: `epoll_wait()` returns events in no
  documented order, so the last of the four is no longer hypothetical.

The default action for `SIGPIPE` is to terminate the process — not to fail the
call, not to drop that one client. The entire server, and every other
connection it is serving, disappears without a log line. That is too large a
consequence to leave resting on scheduling luck when the guard is free.

Socket writes already carry `MSG_NOSIGNAL`; the CGI pipes are the one place
with no equivalent flag, so the signal has to be disarmed process-wide.

## Fix

```cpp
int main(int ac , char **av, char **envp)
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handle_sigint);
    signal(SIGQUIT, handle_sigquit);
    signal(SIGTSTP, handle_sigstp);
    try
    {
		Logging logger("webserv.log");
		int fileNameIdx = ParseLoggingArgs(ac, av);
        open_file(av[fileNameIdx]);
		INFO() << "Opening file: " << av[fileNameIdx];
        validate_file();
        parse_config_file();
        int error_nb = every_server_has_listen_port();
        if (error_nb)
        {
            ERR() << "main: missing listen port at server block " << error_nb
                  << ", a server cannot operate without a listen port";
            return ERROR;
        }
        size_t i = 0;
        Multiplexer Mux;
        while(i < Conf_File::Servers.size())
        {
            size_t j = 0;
            while (j < Conf_File::Servers[i].ports_count)
            {
                Socket *s = new Socket();
                Mux.env = envp;
                s->setup(Conf_File::Servers[i].listen_port[j], Conf_File::Servers[i].host);
                Mux.addServer(s);
                INFO() << "main: listening on " << Conf_File::Servers[i].host
                       << ":" << Conf_File::Servers[i].listen_port[j];
                j++;
            }
            i++;
        }
        Mux.run();
        INFO() << "main: server stopped";
    }
    catch(const std::exception& e)
    {
        ERR() << "main: " << e.what();
    }
    return 0;
}
```

With it in place, a write to a dead CGI returns `-1` and
`Multiplexer::writeCgiInput()` closes the input side and finishes the answer —
the behaviour [08](08-cgi-body-lost-and-deadlock.md) already relies on.

## While you are in this function

Two smaller things, both real:

**The argument count is never checked.** The guard is commented out just above:

```
$ ./webserv
[ERROR] main: basic_string::_M_construct null not valid
$ echo $?
0
```

`av[1]` is `NULL`, `std::string(NULL)` throws, the catch block prints an
unhelpful message and the program reports success. Re-enabling the check gives
a usable message and a non-zero status:

```cpp
        if (ac < 2 || fileNameIdx >= ac)
            throw Error::Argc();
```

**A failed start still returns 0.** Every `catch` path ends at `return 0`, so a
bind failure, a bad configuration file and a clean shutdown are
indistinguishable to a script or a service manager. `return ERROR;` at the end
of the catch block fixes it.
