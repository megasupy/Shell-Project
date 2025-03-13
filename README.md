# Shell-Project
A minimal bash clone created in C using various Linux syscalls.
Created as part of my Systems Software class.

## Features
* Piping with |

  Example: ```ls | grep txt ```
* Redirection of stdin and stdout with < and >

  Example: ```ls >out.txt``` (> or < must be directly to the left of the file name, no spaces)

## Supported Commands
* Anything in the $PATH. This does not yet include cd.
