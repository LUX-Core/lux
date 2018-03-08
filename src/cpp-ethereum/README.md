# cpp-cpp-ethereum - Ethereum C++ client

This repository contains [cpp-cpp-ethereum](http://cpp-cpp-ethereum.org), the [Ethereum](http://cpp-ethereum.org) C++ client.

It is the third most popular of the Ethereum clients, behind [geth](https://github.com/cpp-ethereum/go-cpp-ethereum) (the [go](https://golang.org)
client) and [Parity](https://github.com/ethcore/parity) (the [rust](https://www.rust-lang.org/) client).  The code is exceptionally
[portable](http://cpp-cpp-ethereum.org/portability.html) and has been used successfully on a very broad range
of operating systems and hardware.

We are in the [process of re-licensing](https://bobsummerwill.com/2016/07/12/c-re-licensing-plan/) the codebase from the copyleft
[GPLv3](https://en.wikipedia.org/wiki/GNU_General_Public_License) license to the permissive [Apache 2.0](https://en.wikipedia.org/wiki/Apache_License)
licence, to enable Ethereum to be used as broadly as possible.  There is a long-form
article - ["Ethereum Everywhere"](https://bobsummerwill.com/2016/07/12/cpp-ethereum-everywhere/) - which talks about the rationale for the change and
the history leading up to this proposed change of licensing.

## Getting Started

The Ethereum Documentation site hosts the **[cpp-cpp-ethereum homepage](http://cpp-cpp-ethereum.org)**, which
has a Quick Start section.

Please do come and chat to us on the [cpp-cpp-ethereum](https://gitter.im/cpp-ethereum/cpp-cpp-ethereum) gitter channel if you need help with anything!

                 | Status
-----------------|-----------
Ubuntu and macOS | [![Build Status](https://travis-ci.org/cpp-ethereum/cpp-cpp-ethereum.svg?branch=develop)](https://travis-ci.org/cpp-ethereum/cpp-cpp-ethereum/branches)
Windows          | [![Build Status](https://ci.appveyor.com/api/projects/status/gj9d6wvs08cg85cv/branch/develop?svg=true)](https://ci.appveyor.com/project/cpp-ethereum/cpp-cpp-ethereum)

## Contributing

The current codebase is the work of many, many hands, with nearly 100
[individual contributors](https://github.com/cpp-ethereum/cpp-cpp-ethereum/graphs/contributors) over the course of its development.

Our day-to-day development chat happens on the [cpp-cpp-ethereum-development](https://gitter.im/cpp-ethereum/cpp-cpp-ethereum-development) gitter channel.

All contributions are welcome!  We try to keep a list of tasks that are suitable for
newcomers under the tag [good first task](https://github.com/cpp-ethereum/webthree-umbrella/labels/good%20first%20task).
If you have any questions, please just ask.

Please refer to the file [CONTRIBUTING.md](CONTRIBUTING.md) for some guidelines.

Please read [CodingStandards.txt](CodingStandards.txt) thoroughly before making alterations to the code base.
Please do *NOT* use an editor that automatically reformats whitespace away from astylerc or the formatting guidelines
as described in [CodingStandards.txt](CodingStandards.txt).

All development goes in develop branch.

## Testing

To run the tests, make sure you clone https://github.com/cpp-ethereum/tests and point the environment variable
`ETHEREUM_TEST_PATH` to that path.

## License

All contributions are made under the [GPLv3 license](http://www.gnu.org/licenses/gpl-3.0.en.html). See [LICENSE](LICENSE).

We are in the process of re-licensing to Apache 2.0.   See above for more details.
