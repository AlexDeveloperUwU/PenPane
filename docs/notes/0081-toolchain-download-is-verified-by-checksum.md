# 0081. The toolchain download is verified against a pinned SHA-256

- Status: accepted
- Date: 2026-11-21

## Problem

`setup-toolchain.ps1` downloaded an archive and extracted it into `C:\tools` without checking it. A corrupted or substituted download would have become the compiler used for every build.

## Decision

The script holds the SHA-256 of the pinned archive. After the download it computes the hash; on a mismatch it deletes the file, extracts nothing and stops with both values in the message. Changing the toolchain version means changing the version and the hash together.

## Why

The compiler runs with the privileges of the developer and decides what the executable contains, so the cheapest control is to refuse anything that is not the exact file that was reviewed.

## Alternatives rejected

- Trusting TLS alone: protects the transfer, not the content of the release.
- Publishing the hash next to the archive: the upstream release does not provide one, so the pinned value was computed from the archive the project already used.

## Consequences

The pinned value is trust on first use: it records the archive that produced the existing toolchain and does not prove that archive was what upstream published. Compare it with an independent source when one becomes available.
