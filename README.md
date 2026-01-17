# 🐚 Custom UNIX Shell

A feature-rich, POSIX-compliant shell implementation written in C, featuring advanced command-line editing, intelligent tab completion, pipeline support, and persistent command history.

![Shell Demo](https://img.shields.io/badge/language-C-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-lightgrey.svg)

---

## ✨ Features at a Glance

| Feature                     | Description                                                     |
| --------------------------- | --------------------------------------------------------------- |
| 🎯 **Smart Tab Completion** | Intelligent auto-completion with longest common prefix matching |
| 📜 **Persistent History**   | Command history with file persistence and navigation            |
| 🔀 **Advanced Pipelines**   | Multi-stage pipelines with builtin support                      |
| 📂 **I/O Redirection**      | Full support for stdin/stdout/stderr redirection                |
| ⌨️ **Raw Mode Input**       | Character-by-character input with arrow key navigation          |
| 🔧 **Built-in Commands**    | Essential shell builtins (cd, pwd, echo, type, history, exit)   |

---

## 🎯 Core Features

### Built-in Commands

Our shell implements essential built-in commands that execute directly without spawning new processes:

#### **`echo`** - Display text

```bash
$ echo Hello World
Hello World

$ echo "Quoted text"
Quoted text
```

#### **`cd`** - Change directory

```bash
$ cd /home/user
$ cd ..
$ cd ~           # Go to home directory
$ pwd
/home/user
```

#### **`pwd`** - Print working directory

```bash
$ pwd
/home/user/projects
```

#### **`type`** - Identify command type

```bash
$ type echo
echo is a shell builtin

$ type ls
ls is /usr/bin/ls

$ type nonexistent
nonexistent: not found
```

#### **`history`** - Command history management

```bash
# Display all history
$ history
    1  echo hello
    2  ls -la
    3  history

# Display last N commands
$ history 2
    2  ls -la
    3  history

# Read from file
$ history -r ~/.bash_history

# Write to file
$ history -w ~/my_history.txt

# Append new commands to file
$ history -a ~/my_history.txt
```

#### **`exit`** - Exit the shell

```bash
$ exit
```

---

### 🔍 Tab Completion

Our intelligent tab completion system provides context-aware suggestions:

#### **Single Match Completion**

```bash
$ ec<TAB>
$ echo █
```

#### **Multiple Match with Common Prefix**

```bash
$ xyz_<TAB>
$ xyz_foo█

# Files: xyz_foo, xyz_foo_bar, xyz_foo_baz
# Completes to longest common prefix
```

#### **Ambiguous Matches**

```bash
$ e<TAB>        # Rings bell
$ e<TAB><TAB>   # Shows all matches
echo  env  export
$ e█
```

#### **Progressive Completion**

```bash
# Step 1: Complete to common prefix
$ xyz_<TAB>
$ xyz_foo_

# Step 2: Continue completion
$ xyz_foo_<TAB>
$ xyz_foo_bar_

# Step 3: Single match adds space
$ xyz_foo_bar_<TAB>
$ xyz_foo_bar_baz █
```

**Completion Features:**

- ✅ Searches both built-in commands and PATH executables
- ✅ Removes duplicates from multiple PATH directories
- ✅ Alphabetically sorted suggestions
- ✅ Automatic space after unique completion
- ✅ Bell notification for no/multiple matches

---

### 📜 Command History

Persistent command history with file integration:

#### **Navigation**

```bash
# Use arrow keys to navigate
$ echo first
first
$ echo second
second
$ <UP>          # Shows: echo second
$ <UP>          # Shows: echo first
$ <DOWN>        # Shows: echo second
```

#### **History Storage**

```bash
# Commands are stored in memory (up to 50 commands)
# Rolling window: oldest commands are removed when limit reached
```

#### **File Operations**

**Read from file (`-r`)**

```bash
$ history -r ~/.my_history
# Appends file contents to in-memory history
```

**Write to file (`-w`)**

```bash
$ history -w ~/history_backup.txt
# Overwrites file with all current history
```

**Append to file (`-a`)**

```bash
$ echo command1
$ echo command2
$ history -a ~/history.txt
# Only command1, command2, and history -a are appended

$ echo command3
$ history -a ~/history.txt
# Only command3 and history -a are appended (incremental)
```

#### **HISTFILE Environment Variable**

Set `HISTFILE` to automatically load and save history:

```bash
# Load history on startup, save on exit
$ HISTFILE=~/.my_shell_history ./shell

# History is loaded automatically
$ history
    1  previous_command_1
    2  previous_command_2

# On exit, new commands are appended
$ echo new_command
$ exit
# File now contains both old and new commands
```

**Behavior:**

- On **startup**: Loads commands from `$HISTFILE`
- On **exit**: Appends new commands to `$HISTFILE`
- Preserves existing file content (append mode)

---

### 🔀 Pipelines

Full support for chaining commands with the pipe operator:

#### **Two-Command Pipeline**

```bash
$ cat file.txt | wc -l
42
```

```
┌─────────┐    pipe    ┌─────────┐
│   cat   │──────────▶ │   wc    │
└─────────┘            └─────────┘
```

#### **Multi-Stage Pipeline**

```bash
$ ls -la | grep ".c" | head -n 5 | wc -l
5
```

```
┌─────┐  pipe  ┌──────┐  pipe  ┌──────┐  pipe  ┌─────┐
│ ls  │───────▶│ grep │───────▶│ head │───────▶│ wc  │
└─────┘        └──────┘        └──────┘        └─────┘
```

#### **Built-ins in Pipelines**

```bash
# Built-in as source
$ echo "hello\nworld" | wc -l
2

# Built-in as sink
$ ls | type echo
echo is a shell builtin

# Multiple built-ins
$ echo "test" | type grep | cat
grep is /usr/bin/grep
```

**Pipeline Features:**

- ✅ Unlimited pipeline stages
- ✅ Built-in commands work seamlessly
- ✅ Proper process management and cleanup
- ✅ Correct stdin/stdout/stderr handling
- ✅ Supports streaming data (`tail -f`)

---

### 📂 I/O Redirection

Comprehensive redirection support for flexible command output:

#### **Output Redirection**

```bash
# Redirect stdout
$ echo "Hello" > output.txt
$ echo "Hello" 1> output.txt   # Explicit stdout

# Append stdout
$ echo "World" >> output.txt
$ echo "World" 1>> output.txt  # Explicit stdout
```

#### **Error Redirection**

```bash
# Redirect stderr
$ command_not_found 2> errors.txt

# Append stderr
$ another_error 2>> errors.txt
```

#### **Combined Redirection**

```bash
# Redirect both stdout and stderr
$ complex_command > output.txt 2> errors.txt

# Append both
$ another_command >> output.txt 2>> errors.txt
```

**Redirection Operators:**
| Operator | Description |
|----------|-------------|
| `>` or `1>` | Redirect stdout (overwrite) |
| `>>` or `1>>` | Redirect stdout (append) |
| `2>` | Redirect stderr (overwrite) |
| `2>>` | Redirect stderr (append) |

---

## 🎓 Advanced Usage

### Quote Handling

Our shell properly handles single quotes, double quotes, and escape sequences:

```bash
# Single quotes (literal)
$ echo 'Hello $USER'
Hello $USER

# Double quotes (with escapes)
$ echo "Hello \"World\""
Hello "World"

# Backslash escapes
$ echo Hello\ World
Hello World
```

### Signal Handling

```bash
# Ctrl+C: Cancel current line (doesn't exit)
$ long_command<Ctrl+C>
$ █

# Ctrl+D: Exit shell (EOF)
$ <Ctrl+D>
[shell exits]
```

### Environment Variables

```bash
# PATH is used for executable lookup
$ echo $PATH
/usr/local/bin:/usr/bin:/bin

# HOME is used for cd ~
$ cd ~
$ pwd
/home/user

# HISTFILE for history persistence
$ HISTFILE=~/.my_history ./shell
```

---

## 🏗️ Architecture

### High-Level Design

```
┌─────────────────────────────────────────────┐
│              Main Event Loop                │
│  (Raw mode character-by-character input)    │
└──────────────┬──────────────────────────────┘
               │
       ┌───────┴───────┐
       │               │
    ┌──▼───┐      ┌───▼────┐
    │ TAB  │      │ ENTER  │
    └──┬───┘      └───┬────┘
       │              │
       │              │
  ┌────▼────┐    ┌────▼─────┐
  │Tab      │    │ Parser   │
  │Complete │    │          │
  └─────────┘    └────┬─────┘
                      │
              ┌───────┴────────┐
              │                │
         ┌────▼─────┐    ┌────▼────────┐
         │ Builtin  │    │  Pipeline   │
         │ Command  │    │   Handler   │
         └──────────┘    └─────┬───────┘
                               │
                    ┌──────────┴──────────┐
                    │                     │
               ┌────▼─────┐         ┌────▼────┐
               │ Builtin  │         │ External│
               │ in Pipe  │         │ Command │
               └──────────┘         └─────────┘
```

### Module Breakdown

| Module                | Responsibility                         |
| --------------------- | -------------------------------------- |
| **Tokenizer**         | Parse command strings into argv arrays |
| **Path Resolver**     | Locate executables in PATH directories |
| **Tab Completion**    | Auto-complete with LCP algorithm       |
| **History Manager**   | In-memory and file-based history       |
| **Pipeline Executor** | Multi-stage pipe coordination          |
| **Builtin Handler**   | Execute internal commands              |
| **I/O Redirector**    | File descriptor manipulation           |

---

## 🔧 Technical Details

### Key Technologies

- **Raw Terminal Mode**: Character-by-character input via `termios`
- **ANSI Escape Sequences**: Cursor control and line manipulation
- **Process Management**: `fork()`, `exec()`, `waitpid()`
- **Inter-Process Communication**: `pipe()` for pipeline data flow
- **File Descriptor Manipulation**: `dup2()` for redirection

### Performance Characteristics

| Operation             | Time Complexity                         |
| --------------------- | --------------------------------------- |
| Tab completion search | O(n) where n = PATH entries             |
| History lookup        | O(1) indexed access                     |
| Pipeline creation     | O(k) where k = number of stages         |
| Command execution     | O(1) for builtins, O(fork) for external |

### Memory Management

- **History buffer**: Fixed 50 commands × 1024 bytes = ~50KB
- **Tab completion**: ~256 matches × 256 bytes = ~64KB
- **Input buffer**: 1024 bytes per line
- **Total memory footprint**: ~150KB static allocation

---

## 🎨 Shell Prompt Features

### Interactive Input Handling

```bash
# Backspace support
$ echo hell█o
         ↑ cursor
<BACKSPACE>
$ echo hel█o

# Arrow key navigation
$ <UP>      # Previous command
$ <DOWN>    # Next command
$ <LEFT>    # (not implemented)
$ <RIGHT>   # (not implemented)
```

### Visual Feedback

- **Bell on ambiguous completion**: `\x07`
- **Prompt redraw on completion**: `\r\033[K$ `
- **Clear line on history navigation**

---

## 📚 References

- [POSIX Shell Command Language](https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html)
- [Advanced Programming in the UNIX Environment](https://www.apuebook.com/)
- [The Linux Programming Interface](https://man7.org/tlpi/)

---

## 📞 Contact

**Project Maintainer**: Vaibhav Tyagi  
**Email**: tyagi.vaibhav4814@gmail.com  
**GitHub**: [@its-vaibhav04](https://github.com/its-vaibhav04)

---

<div align="center">

**⭐ Star this repository if you found it helpful! ⭐**

</div>
