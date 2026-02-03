# QScintilla (2.14.1)
QScintilla - a Port to Qt v5 and Qt v6 of Scintilla

---

**docs written by *US***

# Request on the request of requesting me a pull request

Don't send pull request to me. Instead, send them to the [Riverbank computer (see below)](https://riverbankcomputing.com/) for pull request. I'm here for pulling and forking them only.

(However sending pull requests to us, Bright Software Foundation, are very welcome. GitHub is the only official source code repo-lib currently. Don't send pull request to GitLab, SRC, A-Li-Yun because they're not our official one.

# What is a fork?

A fork is being a copycat without paying a buck.

## Is it safe? It is good? It is suitable?

It depends.

## Is it legal?

As long as the forked one is open-source with a license.

## Why BSF do this?

Because we think it's fun.

---

**docs written by *them***

## Introduction

[QScintilla](https://riverbankcomputing.com/software/qscintilla/) is a port to Qt of the [Scintilla](https://www.scintilla.org) editing component.

As well as features found in standard text editing components, Scintilla includes features especially useful when editing and debuggin source code:

* syntax styling with support for over 70 languages
* error indicators
* code completion
* call tips
* code folding
* margins can contain markers like those used in debuggers to indicate breakpoints and the current line.
* recordable macros
* multiple views
* printing.

QScintilla is a port or Scintilla to the Qt GUI toolkit from [The Qt Company](https://qt.io) and runs on any operating system supported by Qt (eg. Windows, Linux, macOS, iOS and Android). QScintilla works with Qt v5 and v6.

QScintilla also includes language bindings for [Python](https://www.python.org). These require that [PyQt](https://riverbankcomputing.com/software/pyqt/) v5 or v6 is also installed.

This version of QScintilla is based on Scintilla v3.10.1.

## Licensing

QScintilla is available under the [GNU General Public License v3](https://gnu.org/licenses/gpl.html) and the Riverbank Commercial License.


The commercial license allows closed source applications using QScintilla to be developed and distributed. At the moment the commercial version of QScintilla is bundled with, but packaged separately from, the commercial version of [PyQt](https://riverbankcomputing.com/software/pyqt/).

The Scintilla code within QScintilla is released under the following license:

```
 License for Scintilla and SciTE

 Copyright 1998-2003 by Neil Hodgson neilh@scintilla.org

 All Rights Reserved

 Permission to use, copy, modify, and distribute this software and its
 documentation for any purpose and without fee is hereby granted,
 provided that the above copyright notice appear in all copies and that
 both that copyright notice and this permission notice appear in
 supporting documentation.

 NEIL HODGSON DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS, IN NO EVENT SHALL NEIL HODGSON BE LIABLE FOR ANY
 SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
 OR PERFORMANCE OF THIS SOFTWARE.
```

## Installation

As supplied QScintilla will be built as a shared library/DLL and installed in the same directories as the Qt libraries and include files.

If you wish to build a static version of the library then pass `CONFIG+=staticlib` on the `qmake` command line.

On macOS, if you wish to build a dynamic version of the library that supports both `x86_64` and `arm64` architectures then edit the file `qscintilla.pro` in the src directory and comment in the definition of `QMAKE_APPLE_DEVICE_ARCHS`. Similar changes can be made to the `.pro` files for the Designer plugin and the example application.

If you want to make more significant changes to the configuration then edit the file `qscintilla.pro` in the `src` directory.

If you do make changes, specifically to the names of the installation directories or the name of the library, then you may also need to update the `src/features/qscintilla2.prf` file.

See your `qmake` documentation for more details.

To build and install QScintilla, run:

```console
    cd src
    qmake
    make
    make install
```

If you have multiple versions of Qt installed then make sure you use the correct version of `qmake`.

The underlying Scintilla code may support additional compile-time options. These can be configured by passing appropriate arguments to `qmake`. For example, if you have an old C++ compiler that does not have a working `std::regex` then invoke `qmake` as follows:

```console
    qmake DEFINES+=NO_CXX11_REGEX=1
```

### Installation on Windows

Before compiling QScintilla on Windows you should remove the `Qsci` directory containing the QScintilla header files from any previous installation. This is because the `Makefile` generated by `qmake` will find these older header files instead of the new ones.

Depending on the compiler you are using you may need to run `nmake` rather than `make`.

If you have built a Windows DLL then you probably also want to run:

```console
    copy %QTDIR%\lib\qscintilla2.dll %QTDIR%\bin
```

## Integration with `qmake`

To configure qmake to find your QScintilla installation, add the following line to your application's .pro file:

```console
    CONFIG += qscintilla2
```

## Qt Designer Plugin

QScintilla includes an optional plugin for Qt Designer that allows QScintilla instances to be included in GUI designs just like any other Qt widget.

To build the plugin on all platforms, make sure QScintilla is installed and then run (as root or administrator):

```console
    cd designer
    qmake
    make
    make install
```

On Windows (and depending on the compiler you are using) you may need to run `nmake` rather than `make`.

## Example Application

The example application provided is a port of the standard Qt `application` example with the [QsciScintilla](https://brdocumentation.github.io/qscintilla/classQsciScintilla.html) class being used instead of Qt's QTextEdit class.

The example does not demonstrate all of the extra features of QScintilla.

To build the example, run:

```console
    cd example
    qmake
    make
```

On Windows (and depending on the compiler you are using) you may need to run `nmake` rather than `make`.

## Python Bindings

The Python bindings are in the `Python` sub-directory. You must have either PyQt5 or PyQt6 already installed and PyQt-builder. QScintilla must also already be built and installed.

The Python sub-directory contains a `pyproject-qt5.toml` file and a `pyproject-qt6.toml` file. If you are building for PyQt5 and Qt v5 then you must copy the `pyproject-qt5.toml` file to `pyproject.toml`. If instead you are building for PyQt6 and Qt v6 then you must copy the `pyproject-qt6.toml` file to `pyproject.toml`.

To build and install the bindings, run:

```console
    cd Python
    sip-install
```
