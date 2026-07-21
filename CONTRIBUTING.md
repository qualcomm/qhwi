# Contributing to QHWI

Thank you for your interest in contributing to QHWI! This document describes the process for submitting contributions.

## Developer Certificate of Origin (DCO)

Before your contribution can be accepted, every commit must be signed off to certify that you have the right to submit the code under this project's license, per the [Developer Certificate of Origin](DCO).

Sign off using the `-s` flag when committing:

```
git commit -s -m "Your commit message"
```

This adds a `Signed-off-by` trailer to your commit message with your name and email. A CI check verifies that all commits in a pull request carry this trailer.

If you forgot to sign off a commit, you can amend it:

```
git commit --amend -s
```

## Getting Started

1. Fork the repository and clone your fork.
2. Create a new branch from `main` for your change:
   ```
   git checkout -b my-feature-branch
   ```
3. Make your changes and commit them with a clear message.
4. Push your branch and open a pull request against `main`.

## Pull Request Guidelines

- Keep pull requests focused — one feature or fix per PR.
- Include a clear description of what the change does and why.
- Link to any related issues.
- Ensure your code builds cleanly and does not break existing tests.
- Follow the coding style described below.

## Coding Style

QHWI uses `.clang-format` to enforce code style for C and C++ files. Before submitting, run clang-format on your changes:

```
clang-format -i <your_files>
```

Python files should follow [PEP 8](https://peps.python.org/pep-0008/).

## Building and Testing

See [README.md](README.md) for full build instructions.

### Supported platforms and compilers

| Platform | Architecture | Compiler | Graphics backends |
|----------|-------------|----------|-------------------|
| Windows  | x64, ARM64  | MSVC 2019+ (19.28+), Clang 17+ | Vulkan, D3D12 |
| Linux    | x64         | Clang 17+ | Vulkan |
| Android  | ARM64 (arm64-v8a) | NDK r25b (23.1.7779620) or newer | Vulkan |

### Build requirements

- **CMake** 3.22.1 or newer (3.26.1+ recommended)
- **Windows:** Visual Studio 2019/2022 or LLVM/Clang with MSVC linker; Vulkan SDK 1.3.239.0+ (for the Vulkan backend)
- **Linux:** Clang 17+, Ninja, Vulkan development headers (`libvulkan-dev`)
- **Android:** Android Studio 2022.3.1.19+, NDK 23.1.7779620+, JDK 17, CMake 3.26.1+
- **Shader compilation:** DXC (downloaded automatically by CMake at configure time via `GetDxc.cmake`)
- **Git LFS** (required to pull test reference images): `git lfs install` before cloning

Run the unit test suite before submitting:

```
cd build/test
ctest -C Debug -j
```

All existing tests must pass. New features should include tests where appropriate.

## Reporting Bugs

Open a GitHub Issue with:
- A clear and descriptive title.
- Steps to reproduce the problem.
- Expected and actual behavior.
- Platform, OS, GPU, and driver version.
- Any relevant logs or screenshots.

## Feature Requests

Feature requests are welcome. Open a GitHub Issue describing the feature and the use case it addresses.

## Code of Conduct

This project follows the [Contributor Covenant Code of Conduct](CODE_OF_CONDUCT.md). By participating, you agree to uphold this standard.
