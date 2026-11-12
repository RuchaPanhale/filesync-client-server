
# FileSync — Performance and Integrity Test Results

## Overview

This document presents additional performance and file-integrity testing conducted on FileSync, a TCP client-server file synchronization system implemented in C as part of a three-member Systems Programming group project.

The objective was to evaluate FileSync's ability to transfer binary files of increasing sizes using chunked file transfers, verify data integrity using SHA-256 checksums, and measure approximate upload and download throughput.

**Test date:** October 31, 2026  
**Environment:** Apple MacBook Pro (M2), macOS; client and server running locally over `127.0.0.1` (localhost)  
**Transfer buffer:** 1,024 bytes (1 KiB)  
**Integrity verification:** SHA-256 checksums  
**Benchmark method:** `/usr/bin/time -p`

---

## 1. File Transfer and Integrity Tests

Random binary files were generated using `/dev/urandom` at three sizes: 1 MiB, 10 MiB, and 100 MiB.

Each file was uploaded using the PUSH command. The original client file was then compared against the server-side copy using SHA-256 checksums.

The files were subsequently downloaded using the PULL command, and the downloaded copies were compared against preserved originals to verify data integrity.

### Results

| File Size | PUSH | Upload SHA-256 | PULL | Download SHA-256 |
|---|---|---|---|---|
| 1 MiB | Passed | Matched | Passed | Matched |
| 10 MiB | Passed | Matched | Passed | Matched |
| 100 MiB | Passed | Matched | Passed | Matched |

**Largest successfully tested file:** 100 MiB (104,857,600 bytes, approximately 104.86 MB).

All three file sizes passed both upload and download integrity verification, with matching SHA-256 checksums.

### Reproduction Instructions

Run the following commands from the project root directory after compiling the client and server with `make`. The `client_files/` and `server_files/` directories must exist.

**1. Generate a 100 MiB binary test file**

```bash
dd if=/dev/urandom of=client_files/test_100mb.bin bs=1048576 count=100
```

**2. Start the server**

```bash
./server 8080
```

Keep the server running and open another terminal in the project directory for the following commands.

**3. Upload the file**

```bash
./client 127.0.0.1 8080 client1 push test_100mb.bin
```

**4. Verify upload integrity**

```bash
shasum -a 256 client_files/test_100mb.bin server_files/test_100mb.bin
```

Both files should produce identical SHA-256 checksums.

**5. Preserve the original file before downloading**

```bash
cp client_files/test_100mb.bin client_files/test_100mb_original.bin
```

**6. Download the file**

```bash
./client 127.0.0.1 8080 client1 pull test_100mb.bin
```

**7. Verify download integrity**

```bash
shasum -a 256 client_files/test_100mb_original.bin client_files/test_100mb.bin
```

Matching SHA-256 values provide strong evidence that the compared files are byte-identical.

---

## 2. Transfer Performance Benchmark

Upload and download performance were measured using a 100 MiB binary file over localhost.

Each operation was timed three times using `/usr/bin/time -p`.

The `real` value represents elapsed wall-clock time for the client command, including socket communication, file transfer, and command-processing overhead.

### Benchmark Results

| Run | PUSH Time (s) | PULL Time (s) |
|---|---:|---:|
| 1 | 0.26 | 0.28 |
| 2 | 0.30 | 0.33 |
| 3 | 0.26 | 0.28 |
| **Average** | **0.273** | **0.297** |

### Average Transfer Throughput

Average throughput was calculated using:

`Average throughput = Total data transferred / Total elapsed time`

Each operation transferred 100 MiB of data per run.

| Metric | PUSH | PULL |
|---|---:|---:|
| Total transferred | 300 MiB | 300 MiB |
| Total elapsed time | 0.82 s | 0.89 s |
| Average throughput | **365.9 MiB/s** | **337.1 MiB/s** |

These measurements represent approximate application-level throughput over localhost, not physical network bandwidth.

### Benchmark Commands

**Timed upload**

```bash
/usr/bin/time -p ./client 127.0.0.1 8080 client1 push test_100mb.bin
```

**Timed download**

```bash
/usr/bin/time -p ./client 127.0.0.1 8080 client1 pull test_100mb.bin
```

For an existing file, the client must first acquire its lock through PULL before pushing an updated version. A successful PUSH releases the lock.

Repeated PULL measurements require releasing the previous lock using DONE or completing an authorized PUSH.

---

## 3. File Locking and Status Verification

In addition to performance and integrity testing, FileSync's locking and status functionality was exercised during development and benchmarking.

The following behaviors were verified:

- Successful PULL operations acquire a file lock for the requesting client.
- PULL attempts by a different client are rejected while the file is locked.
- Existing-file PUSH operations require the client to own the corresponding lock.
- Successful PUSH operations release the file lock.
- The STATUS command displays server file information, including lock status, lock ownership, and file size.
- The DONE command releases locks held by a specified client.

### Multiple-Lock Release Test

During testing, the server returned the following response:

```text
OK released 3 lock(s) for client client1
```

This verified that a single DONE command could release multiple file locks held by the same client.

The server processes incoming client connections sequentially; these tests verify logical lock ownership and access restrictions rather than parallel transfer performance.

---

## 4. Bug Fix: Intermittent Download Corruption

**Found:** November 2026, during post-project code review.

**Symptom:** PULL could save an incomplete file while still printing "Download complete."

**Root cause:** The server sent the `OK <filesize>` header and the file data back-to-back on the same TCP stream, with no delimiter after the header. The client read the header with a single `recv()` of up to 1,023 bytes, so when file data had already arrived, the first part of the file was consumed as part of the header and never written to disk. The client also ignored the return value of `receive_file()`, so the failure was silent.

**Why earlier tests passed:** On localhost (M2 Mac), the header usually arrived alone before any file data, so the bug did not appear in normal runs. It appears when the client reads more slowly than the server sends, for example under network latency or system load.

**Reproduction (fault injection):** A temporary 200 ms delay (`usleep(200000)`) was added before the client reads the server's response to simulate a slow network. Each file was pulled 20 times and compared against a preserved original using `cmp`.

| File Size | Before Fix (200 ms delay) | After Fix (200 ms delay) |
|---|---|---|
| 1 MiB | 20/20 corrupted | 0/20 corrupted |
| 10 MiB | 20/20 corrupted | 0/20 corrupted |
| 100 MiB | 20/20 corrupted | 0/20 corrupted |

The delay was removed after verification.

**Fix:**
- The server now ends the PULL header with a newline (`OK <filesize>\n`).
- The client reads the header with `recv_line()`, one byte at a time up to the newline, so it never consumes file data.
- The client checks the return values of `send()` and `receive_file()` and reports a failed download instead of printing "Download complete."
- `recv_line()` now returns a partial line if the connection closes, so server `ERROR` messages without a trailing newline still reach the client. (This regression was found while testing the fix.)

---

## 5. Security Fix: Path Traversal

**Found:** November 2026, during post-project code review.

**Issue:** The server built file paths by appending the client-supplied filename to `server_files/` without validation. A request such as `PULL attacker ../../secret_test.txt` read a file outside the server directory, and a PUSH with `../evil.txt` could write outside it.

**Verification:**

| Test | Before Fix | After Fix |
|---|---|---|
| PULL `../../secret_test.txt` (raw socket request) | File contents returned | `ERROR invalid filename` |
| PUSH `../evil.txt` (raw socket request) | Not tested before fix | `ERROR invalid filename`, no file written |
| Normal PULL/DONE | Worked | Worked |

**Fix:** Added `is_safe_filename()` to the server, which rejects empty names, names containing `/`, and the special names `.` and `..`. It is checked at the start of both PULL and PUSH handling.

---

## Re-run Benchmark After Fixes

After both fixes, the 100 MiB benchmark was re-run with 10 PULL/PUSH cycles, timed with Python's `time.perf_counter()`:

| Metric | PULL | PUSH |
|---|---:|---:|
| Average time | 0.221 s | 0.241 s |
| Average throughput | **453.3 MiB/s** | **415.6 MiB/s** |

SHA-256 checksums of the original, downloaded and server copies matched. These numbers are higher than the earlier 3-run results mainly because of the different timing method and OS file caching, not because the fixes improved speed.

---

## 6. Limitations

**Testing scope**
- All benchmarks ran on a single M2 MacBook Pro over localhost, so results reflect OS caching and local hardware rather than real network bandwidth.
- Parallel multi-client throughput and interrupted-transfer recovery were not tested; the server handles connections sequentially.
- SHA-256 integrity checks were run manually and are not built into the application. Large test files are not included in this repository.

**Known design issues (identified, not fixed)**
- Lock state is kept in memory and lost on server restart; locks never expire if a client disconnects without sending DONE.
- Client IDs are self-declared with no authentication, so any client can release another client's locks.
- An interrupted PUSH can leave a partial file on the server, and a rejected PUSH still transmits the full file before reading the error.

---

## Conclusion

FileSync successfully transferred binary files of 1 MiB, 10 MiB, and 100 MiB in both directions, with matching SHA-256 checksums providing evidence of data integrity.

In three-run localhost benchmarks using 100 MiB files, the system achieved average application-level throughput of approximately:

- **365.9 MiB/s for uploads (PUSH)**
- **337.1 MiB/s for downloads (PULL)**

These tests demonstrate successful binary file transfers with verified data integrity and provide baseline localhost performance measurements. Results may vary depending on hardware, operating-system caching, and system load.
