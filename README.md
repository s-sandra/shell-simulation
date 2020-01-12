
# Tiny Shell
This Unix shell emulation borrows the [template]( https://www.cs.utexas.edu/~dahlin/Classes/439/labs/shlab.pdf) created by Mike Dahlin 
for his Principles of Computer Systems course. It has the following simplified interface for job control.

## Supported Functionality
|function|purpose|
|---|---|
`quit`|Exits the shell program.|
`jobs`|Lists all background jobs, both running and stopped.|
`ctrl+c`|Interrupts all current foreground processes.|
`ctrl+z`|Stops all current foreground processes.|
`fg (<JID>\|%<PID>)`|Makes a running or stopped background process run in the foreground.|
`bg (<JID>\|%<PID>)`| Makes a stopped foreground process run in the background. You can also make a specified executable file run in the background by appending `&` to the end of the command.|
`kill <JID>`|Terminates the job with the specified ID.|

## Sample Programs
If the provided command is not built-in, than the tiny shell assumes it is a path to an executable file and runs it using any 
given arguments. There are several tester programs provided. These include myspin, mykill, myint and mysplit.

## Sample Output
```
tsh> ./bogustsh> ./bogus
./bogus: Command not found.
tsh> ./myspin 10
Job [1] (2792) terminated by signal 2
tsh> ./myspin 3 &
[1] (2794) ./myspin 3 &
tsh> ./myspin 4 &
[2] (2796) ./myspin 4 &
tsh> jobs
[1] (2794) Running ./myspin 3 &
[2] (2796) Running ./myspin 4 &
tsh> fg %1
Job [1] (2794) stopped by signal: 20
tsh> jobs
[1] (2794) Stopped ./myspin 3 &
[2] (2796) Running ./myspin 4 &
tsh> bg %3
bg %(3): No such job
tsh> bg %1
[1] (2794) ./myspin 3 &
tsh> jobs
[1] (2794) Running ./myspin 3 &
[2] (2796) Running ./myspin 4 &
tsh> fg %1
tsh> quit
```
More test cases, as well as a shell driver, are provided at https://github.com/zacharski/simpleShell.
